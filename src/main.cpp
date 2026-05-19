#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <libusb-1.0/libusb.h>

#define WIDTH 800
#define HEIGHT 100

typedef struct RenderObj {
    SDL_Texture *texture;
    bool is_shared;
    int x;
    int y;
    int w;
    int h;
} RenderObj;

typedef struct USBEntry {
    unsigned char name[128];
    int nameSize;
    int speed;
} USBEntry;

typedef struct AppData {
    libusb_context *ctx;
    bool *needsRefresh;
    SDL_Renderer *renderer;
    SDL_Window *window;
    TTF_Font *font;
    SDL_Texture *usbTexture;
    SDL_Texture *hidTexture;
    SDL_Texture *storageTexture;
    SDL_Texture *webcamTexture;
    double scrollY;
    bool isDraggingScroll = false;
    int scrollDragOffset = 0;
    SDL_Rect scrollRect;
    std::vector<RenderObj *> objs;
    std::vector<USBEntry *> entries;
} AppData;

void initialize(AppData *data_ptr);
void *createSharedMemory(size_t size);
void handleEvent(SDL_Event event, AppData *data_ptr);
int usbHotplugHandler(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);
void render(AppData *data_ptr);
void refreshDeviceView(AppData *data_ptr);
void listDevices(AppData *data_ptr);
bool compareDeviceEntries(const USBEntry *a, const USBEntry *b);
bool pointInRect(int x, int y, SDL_Rect &rect);
void destroyView(AppData *data_ptr);
void quit(AppData *data_ptr);

int main(int argc, char *argv[]) {
    // Initialize SDL2 (including image and font loaders)
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    // Initialize application
    AppData data;
    initialize(&data);

    // Perform render loop
    SDL_Event event;
    bool wasEvent = true;
    do {
        if (wasEvent) {
            if (*data.needsRefresh == true) *data.needsRefresh = false;
            render(&data);
            handleEvent(event, &data);
        }
        wasEvent = SDL_WaitEventTimeout(&event, 50);
        if (*data.needsRefresh == true) {
            refreshDeviceView(&data);
            wasEvent = true;
        }
    } while (event.type != SDL_QUIT);

    // Clean up
    quit(&data);
    SDL_DestroyRenderer(data.renderer);
    SDL_DestroyWindow(data.window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    libusb_exit(data.ctx);

    return 0;
}

void initialize(AppData *data_ptr) {
    // Create window and renderer
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &data_ptr->window, &data_ptr->renderer);

    // Register hotplug event handler
    if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        // Create shared memory for event flag
        data_ptr->needsRefresh = reinterpret_cast<bool *>(createSharedMemory(sizeof(bool)));
        // Create new process to notify main process about hotplugs
        int pid = fork();
        if (pid == 0) {
            libusb_context *ctx;
            libusb_init_context(&ctx, NULL, 0);
            libusb_hotplug_register_callback(ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, usbHotplugHandler, NULL, NULL);
            while (true) {
                libusb_handle_events(ctx);
                *data_ptr->needsRefresh = true;
            }
            exit(0);
        }
    } else printf("Error: hotplug event handling not supported on this system\n");

    // Initialize libusb
    int err = libusb_init_context(&data_ptr->ctx, NULL, 0);
    if (err == 0) printf("libusb initialized\n");
    else printf("There was an issue initializing libusb: %s\n", libusb_strerror(err));

    // Load font
    data_ptr->font = TTF_OpenFont("resrc/fonts/Inter.ttf", 16);

    // Load images and create textures
    SDL_Surface *usb_img_surf = IMG_Load("resrc/img/USB.png");
    data_ptr->usbTexture = SDL_CreateTextureFromSurface(data_ptr->renderer, usb_img_surf);
    SDL_FreeSurface(usb_img_surf);

    // Create initial view
    data_ptr->scrollY = 0;
    refreshDeviceView(data_ptr);
}

void *createSharedMemory(size_t size) {
    int protection = PROT_READ | PROT_WRITE;
    int visibility = MAP_SHARED | MAP_ANONYMOUS;
    return mmap(NULL, size, protection, visibility, -1, 0);
}

void handleEvent(SDL_Event event, AppData *data_ptr) {
    bool clickIsScroll = event.type == SDL_MOUSEMOTION && event.motion.state > 0;
    if (event.type == SDL_MOUSEWHEEL || clickIsScroll) {
        // Scroll
        int yBound = data_ptr->entries.size() * 32 - HEIGHT;
        float targetScroll;
        if (event.type == SDL_MOUSEWHEEL) {
            // With wheel
            targetScroll = data_ptr->scrollY - event.wheel.y * 32;
        } else if (data_ptr->isDraggingScroll || pointInRect(event.motion.x, event.motion.y, data_ptr->scrollRect)) {
            // By dragging
            data_ptr->isDraggingScroll = true;
            if (data_ptr->scrollDragOffset == 0) data_ptr->scrollDragOffset = event.motion.y - data_ptr->scrollRect.y;
            targetScroll = (float)(event.motion.y - data_ptr->scrollDragOffset) / (HEIGHT - data_ptr->scrollRect.h) * yBound;
        } else {
            SDL_Rect scrollBox;
            scrollBox.w = 11;
            scrollBox.h = HEIGHT;
            scrollBox.x = WIDTH - 13;
            scrollBox.y = 0;
            if (pointInRect(event.motion.x, event.motion.y, scrollBox)) {
                // Directly to pointer
                data_ptr->scrollDragOffset = (float)data_ptr->scrollRect.h / 2;
                targetScroll = ((float)event.motion.y - data_ptr->scrollDragOffset) / (HEIGHT - data_ptr->scrollRect.h) * yBound;
            } else return;
        }
        data_ptr->scrollY = fmax(0, fmin(targetScroll, yBound));
    } else {
        data_ptr->isDraggingScroll = false;
        data_ptr->scrollDragOffset = 0;
    }
}

int usbHotplugHandler(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data) {
    return 0;
}

void render(AppData *data_ptr) {
    // Erase prior frame content (to light gray)
    SDL_SetRenderDrawColor(data_ptr->renderer, 235, 235, 235, 255);
    SDL_RenderClear(data_ptr->renderer);

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
        rect.y = obj->y - data_ptr->scrollY;
        SDL_RenderCopy(data_ptr->renderer, obj->texture, NULL, &rect);
    }

    // Draw separator lines
    SDL_SetRenderDrawColor(data_ptr->renderer, 0, 128, 128, 255);
    for (int i = 1; i < data_ptr->entries.size(); i++) {
        int y = 32 * i - data_ptr->scrollY;
        SDL_RenderDrawLineF(data_ptr->renderer, 0, y, WIDTH - 16, y);
    }

    // Draw scroll bar
    float scrollPercent = (float)data_ptr->scrollY / (data_ptr->entries.size() * 32);
    float screenPercent = (float)HEIGHT / (data_ptr->entries.size() * 32);
    data_ptr->scrollRect.w = 11;
    data_ptr->scrollRect.h = screenPercent * HEIGHT;
    data_ptr->scrollRect.x = WIDTH - 13;
    data_ptr->scrollRect.y = scrollPercent * HEIGHT;
    SDL_SetRenderDrawColor(data_ptr->renderer, 128, 128, 128, 255);
    SDL_RenderFillRect(data_ptr->renderer, &data_ptr->scrollRect);

    // Display rendered frame
    SDL_RenderPresent(data_ptr->renderer);
}

void refreshDeviceView(AppData *data_ptr) {
    destroyView(data_ptr);

    // Get entries
    listDevices(data_ptr);

    // Create textures for entries
    SDL_Color color = { 0, 0, 0 };
    SDL_Color bgColor = { 235, 235, 235 };
    for (int i = 0; i < data_ptr->entries.size(); i++) {
        USBEntry *entry = data_ptr->entries.at(i);
        int y = 5 + 32 * i;
        // Entry icon
        RenderObj *iconObj = new RenderObj();
        iconObj->texture = data_ptr->usbTexture;
        iconObj->is_shared = true;
        iconObj->x = 10;
        iconObj->y = y + 2;
        iconObj->w = 16;
        iconObj->h = 16;
        data_ptr->objs.push_back(iconObj);
        // Entry name
        RenderObj *nameObj = new RenderObj();
        const char *text = reinterpret_cast<const char *>(entry->name);
        SDL_Surface *txt_surf = TTF_RenderUTF8_LCD(data_ptr->font, text, color, bgColor);
        nameObj->texture = SDL_CreateTextureFromSurface(data_ptr->renderer, txt_surf);
        SDL_FreeSurface(txt_surf);
        nameObj->x = 36;
        nameObj->y = y;
        data_ptr->objs.push_back(nameObj);
        // File size
        RenderObj *sizeObj = new RenderObj();
        std::string label = "";
        switch (entry->speed) {
        case 1: label = "1.5Mb/s"; break;
        case 2: label = "12Mb/s"; break;
        case 3: label = "480Mb/s"; break;
        case 4: label = "5Gb/s"; break;
        case 5: label = "10Gb/s"; break;
        case 6: label = "20Gb/s"; break;
        default: label = "Unknown speed";
        }
        txt_surf = TTF_RenderUTF8_LCD(data_ptr->font, label.c_str(), color, bgColor);
        sizeObj->texture = SDL_CreateTextureFromSurface(data_ptr->renderer, txt_surf);
        SDL_FreeSurface(txt_surf);
        sizeObj->x = 400;
        sizeObj->y = y;
        data_ptr->objs.push_back(sizeObj);
    }
}

void listDevices(AppData *data_ptr) {
    // Clear out old data
    data_ptr->entries.clear();

    // Get entries
    libusb_device **dev;
    int length = libusb_get_device_list(data_ptr->ctx, &dev);
    for (int i = 0; i < length; i++) {
        USBEntry *usbEntry = new USBEntry();
        libusb_device_descriptor desc;
        int err = libusb_get_device_descriptor(dev[i], &desc);
        if (err) {
            printf("Error getting descriptor on device %d: %s\n", i, libusb_strerror(err));
            continue;
        }
        libusb_device_handle *handle;
        err = libusb_open(dev[i], &handle);
        if (err) {
            printf("Error opening device %d: %s\n", i, libusb_strerror(err));
            continue;
        }
        int res = libusb_get_string_descriptor_ascii(handle, desc.iProduct, usbEntry->name, 128);
        if (res < 0) {
            printf("Error reading descriptor on device %d: %s\n", i, libusb_strerror(res));
            libusb_close(handle);
            continue;
        } else {
            usbEntry->nameSize = res;
            usbEntry->speed = libusb_get_device_speed(dev[i]);
            data_ptr->entries.push_back(usbEntry);
            libusb_close(handle);
        }
    }
    libusb_free_device_list(dev, length);
    // Sort alphabetically (directories first)
    std::sort(data_ptr->entries.begin(), data_ptr->entries.end(), compareDeviceEntries);
    for (int i = 0; i < data_ptr->entries.size(); i++) {
        std::cout << data_ptr->entries.at(i)->name << std::endl;
    }
}

bool compareDeviceEntries(const USBEntry *a, const USBEntry *b) {
    for (int i = 0; i < 128; i++) {
        if (i >= a->nameSize || i >= b->nameSize) break;
        if (a->name[i] < b->name[i]) return true;
        else if (a->name[i] > b->name[i]) return false;
    }
    // Shortest string is first
    if (a->nameSize > b->nameSize) return true;
    else return false;
}

bool pointInRect(int x, int y, SDL_Rect &rect) {
    if (x > rect.x && x < rect.x + rect.w &&
        y > rect.y && y < rect.y + rect.h) {
        return true;
    }
    return false;
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
    SDL_DestroyTexture(data_ptr->usbTexture);
    TTF_CloseFont(data_ptr->font);
}
