#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#define WIDTH 800
#define HEIGHT 600

typedef struct FileEntry {
    std::string name;
    bool is_directory;
    uint64_t size;
} FileEntry;

typedef struct RenderObj {
    SDL_Texture *texture;
    bool is_shared;
    int x;
    int y;
    int w;
    int h;
} RenderObj;

typedef struct AppData {
    TTF_Font *font;
    std::vector<RenderObj *> objs;
    SDL_Texture *file_texture;
    SDL_Texture *folder_texture;
    SDL_Texture *parent_texture;
    std::filesystem::path current_directory;
    std::vector<FileEntry *> entries;
} AppData;

void initialize(SDL_Renderer *renderer, AppData *data_ptr);
void handleEvent(SDL_Event event, SDL_Renderer *renderer, AppData *data_ptr);
void render(SDL_Renderer *renderer, AppData *data_ptr);
void refreshFileView(SDL_Renderer *renderer, AppData *data_ptr);
void listFiles(std::filesystem::path directory, std::vector<FileEntry *> *entries);
bool compareFileEntries(const FileEntry *a, const FileEntry *b);
void destroyView(AppData *data_ptr);
void quit(AppData *data_ptr);

int main(int argc, char *argv[]) {
    // File browser starts at user's home directory
    std::filesystem::path home(getenv("HOME"));
    std::cout << "HOME: " << home << std::endl;

    // Initialize SDL2 (including image and font loaders)
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    // Create window and renderer
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer);

    // Initialize file browser application
    AppData data;
    data.current_directory = home;
    initialize(renderer, &data);

    // Perform render loop
    SDL_Event event;
    do {
        render(renderer, &data);
        SDL_WaitEvent(&event);
        handleEvent(event, renderer, &data);
    } while (event.type != SDL_QUIT);

    // Clean up
    quit(&data);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}

void initialize(SDL_Renderer *renderer, AppData *data_ptr) {
    // Load font
    data_ptr->font = TTF_OpenFont("resrc/fonts/font.ttf", 16);

    // Load images and create textures
    SDL_Surface *file_img_surf = IMG_Load("resrc/images/file.svg");
    data_ptr->file_texture = SDL_CreateTextureFromSurface(renderer, file_img_surf);
    SDL_FreeSurface(file_img_surf);
    SDL_Surface *folder_img_surf = IMG_Load("resrc/images/folder.png");
    data_ptr->folder_texture = SDL_CreateTextureFromSurface(renderer, folder_img_surf);
    SDL_FreeSurface(folder_img_surf);
    SDL_Surface *parent_img_surf = IMG_Load("resrc/images/parent.svg");
    data_ptr->parent_texture = SDL_CreateTextureFromSurface(renderer, parent_img_surf);
    SDL_FreeSurface(parent_img_surf);

    // Create initial view
    refreshFileView(renderer, data_ptr);
}

void handleEvent(SDL_Event event, SDL_Renderer *renderer, AppData *data_ptr) {
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int entryIndex = (event.button.y - 10) / 32;
        if (entryIndex > -1 && entryIndex < data_ptr->entries.size()) {
            FileEntry *entry = data_ptr->entries.at(entryIndex);
            if (entry->is_directory) {
                data_ptr->current_directory /= entry->name;
                refreshFileView(renderer, data_ptr);
            } else {
                int pid = fork();
                if (pid == 0) {
                    // Child process (open file)
                    std::filesystem::path filePath = data_ptr->current_directory.append(entry->name);
                    char *args[2];
                    args[0] = "/usr/bin/xdg-open";
                    args[1] = const_cast<char *>(filePath.c_str());
                    args[2] = NULL;
                    int result = execv("/usr/bin/xdg-open", args);
                    std::cout << "xdg-open error" << std::endl;
                    exit(1);
                }
            }
        }
    } else if (event.type == SDL_MOUSEWHEEL) {
        for (int i = 0; i < data_ptr->objs.size(); i++) {
            data_ptr->objs.at(i)->y += event.wheel.y * 32;
        }
    }
}

void render(SDL_Renderer *renderer, AppData *data_ptr) {
    // Erase prior frame content (to light gray)
    SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
    SDL_RenderClear(renderer);

    // Draw textures
    for (int i = 0; i < data_ptr->objs.size(); i++) {
        RenderObj *obj = data_ptr->objs.at(i);
        SDL_Rect rect;
        if (obj->w == NULL or obj->h == NULL) {
            SDL_QueryTexture(obj->texture, NULL, NULL, &(rect.w), &(rect.h));
        } else {
            rect.w = obj->w;
            rect.h = obj->h;
        }
        rect.x = obj->x;
        rect.y = obj->y;
        SDL_RenderCopy(renderer, obj->texture, NULL, &rect);
    }

    // Draw solid rectangle (teal)
    // rect.x = 440;
    // rect.y = 320;
    // rect.w = 40;
    // rect.h = 30;
    // SDL_SetRenderDrawColor(renderer, 0, 128, 128, 255);
    // SDL_RenderFillRect(renderer, &rect);

    // Display rendered frame
    SDL_RenderPresent(renderer);
}

void refreshFileView(SDL_Renderer *renderer, AppData *data_ptr) {
    destroyView(data_ptr);
    // Get entries in current directory
    listFiles(data_ptr->current_directory, &data_ptr->entries);
    // Create textures for file entries
    SDL_Color color = { 0, 0, 0 };
    SDL_Color bgColor = { 235, 235, 235 };
    for (int i = 0; i < data_ptr->entries.size(); i++) {
        FileEntry *entry = data_ptr->entries.at(i);
        int y = 10 + 32 * i;
        // Entry icon
        RenderObj *iconObj = new RenderObj();
        if (data_ptr->entries.at(i)->is_directory) iconObj->texture = data_ptr->folder_texture;
        else iconObj->texture = data_ptr->file_texture;
        iconObj->is_shared = true;
        iconObj->x = 10;
        iconObj->y = y + 2;
        iconObj->w = 16;
        iconObj->h = 16;
        data_ptr->objs.push_back(iconObj);
        // Entry name
        RenderObj *nameObj = new RenderObj();
        std::string text = entry->name;
        SDL_Surface *txt_surf = TTF_RenderUTF8_LCD(data_ptr->font, text.c_str(), color, bgColor);
        nameObj->texture = SDL_CreateTextureFromSurface(renderer, txt_surf);
        SDL_FreeSurface(txt_surf);
        nameObj->x = 36;
        nameObj->y = y;
        data_ptr->objs.push_back(nameObj);
        // File size
        if (!entry->is_directory) {
            RenderObj *sizeObj = new RenderObj();
            int sizeVal = entry->size;
            std::string unit = "B";
            if (sizeVal >= 1000) {
                sizeVal /= 1000;
                unit = "KB";
            }
            if (sizeVal >= 1000) {
                sizeVal /= 1000;
                unit = "MB";
            }
            if (sizeVal >= 1000) {
                sizeVal /= 1000;
                unit = "GB";
            }
            std::string text = std::to_string(sizeVal) + " " + unit;
            SDL_Surface *txt_surf = TTF_RenderUTF8_LCD(data_ptr->font, text.c_str(), color, bgColor);
            sizeObj->texture = SDL_CreateTextureFromSurface(renderer, txt_surf);
            SDL_FreeSurface(txt_surf);
            sizeObj->x = 400;
            sizeObj->y = y;
            data_ptr->objs.push_back(sizeObj);
        }
    }
}

void listFiles(std::filesystem::path directory, std::vector<FileEntry *> *entries) {
    // Clear out old data
    entries->clear();

    // Get entries in directory
    for (auto const &dir_entry : std::filesystem::directory_iterator{ directory }) {
        auto name = dir_entry.path().filename();
        if (name.c_str()[0] != '.') {
            FileEntry *fileEntry = new FileEntry();
            fileEntry->name = name;
            fileEntry->is_directory = dir_entry.is_directory();
            fileEntry->size = 0;
            if (!dir_entry.is_directory()) fileEntry->size = dir_entry.file_size();
            entries->push_back(fileEntry);
            // if (dir_entry.is_symlink()) printf(" 🔗\n");
        }
    }
    // Sort alphabetically (directories first)
    std::sort(entries->begin(), entries->end(), compareFileEntries);
    for (int i = 0; i < entries->size(); i++) {
        std::cout << entries->at(i)->name << std::endl;
    }
}

bool compareFileEntries(const FileEntry *a, const FileEntry *b) {
    return a->name < b->name;
}

void destroyView(AppData *data_ptr) {
    for (int i = 0; i < data_ptr->objs.size(); i++) {
        RenderObj *obj = data_ptr->objs.at(i);
        if (!obj->is_shared) SDL_DestroyTexture(obj->texture);
    }
    data_ptr->objs.clear();
}

void quit(AppData *data_ptr) {
    destroyView(data_ptr);
    SDL_DestroyTexture(data_ptr->file_texture);
    SDL_DestroyTexture(data_ptr->folder_texture);
    TTF_CloseFont(data_ptr->font);
}
