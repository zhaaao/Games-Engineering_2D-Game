#pragma once
#include "GamesEngineeringBase.h"
#include "gfx_utils.h"
#include "blit.h"
#include <string>
#include <fstream>
#include <sstream>
using namespace GamesEngineeringBase;
using namespace std;

/**********************************  TileMap  **********************************
 * Basic tile map loader and renderer.
 * Responsible for:
 *   - Loading a tile layout from a text file (tiles.txt format)
 *   - Lazy-loading tile images from a folder (e.g., "./tiles/14.png")
 *   - Rendering tiles around the camera position
 *   - Handling wrapping (infinite map behavior)
 *
 * Each tile ID corresponds to an image file with the same name.
 * For example, tile ID 17 loads "tiles/17.png" when first used.
 *******************************************************************************/
class TileMap {
private:
    int width = 0, height = 0;         // Map dimensions (in tiles)
    int tileW = 32, tileH = 32;        // Tile size (in pixels)
    int* data = nullptr;               // Tile ID array (row-major order)

    // --- Tile image cache ---
    static const int MAX_TILE_ID = 1024;    // Maximum number of unique tile IDs
    GamesEngineeringBase::Image* tileImg[MAX_TILE_ID];
    char folder[260];                      // Resource folder (e.g., "./tiles/")
    bool wrap = false;                     // If true, map repeats infinitely

    // Initializes the image cache to a clean state
    void initCache() {
        for (int i = 0; i < MAX_TILE_ID; ++i) tileImg[i] = nullptr;
        folder[0] = '\0';
    }

    // Parses all integer values from a line (supports commas, spaces, etc.)
    static int parseInts(const std::string& line, int* dst, int maxWrite) {
        int written = 0;
        int i = 0, n = (int)line.size();
        while (i < n && written < maxWrite) {
            while (i < n && !(line[i] == '-' || (line[i] >= '0' && line[i] <= '9'))) ++i;
            if (i >= n) break;
            int sign = 1;
            if (line[i] == '-') { sign = -1; ++i; }
            long val = 0;
            bool hasDigit = false;
            while (i < n && (line[i] >= '0' && line[i] <= '9')) {
                val = val * 10 + (line[i] - '0');
                ++i; hasDigit = true;
            }
            if (hasDigit) dst[written++] = (int)(sign * val);
            while (i < n && (line[i] == ',' || line[i] == ' ' || line[i] == '\t' || line[i] == '\r')) ++i;
        }
        return written;
    }

    // Lazy-loads the tile image for a given ID the first time it’s needed
    // Builds filename as "<folder><id>.png"
    Image* getTileImage(int id) {
        if (id < 0 || id >= MAX_TILE_ID) return nullptr;
        if (tileImg[id]) return tileImg[id];

        // Construct filename manually to avoid dependencies like sprintf
        char filename[320];
        int n = 0;
        for (; folder[n] && n < 259; ++n) filename[n] = folder[n];

        // Append numeric ID
        if (id == 0) filename[n++] = '0';
        else {
            int tmp = id, digits[10], k = 0;
            while (tmp > 0 && k < 10) { digits[k++] = tmp % 10; tmp /= 10; }
            for (int j = k - 1; j >= 0; --j) filename[n++] = char('0' + digits[j]);
        }

        // Append ".png"
        filename[n++] = '.'; filename[n++] = 'p'; filename[n++] = 'n'; filename[n++] = 'g';
        filename[n] = '\0';

        // Attempt load
        Image* img = new Image();
        if (!img->load(string(filename))) {
            delete img;
            tileImg[id] = nullptr; // Remember failure to avoid repeated attempts
            return nullptr;
        }
        tileImg[id] = img;
        return img;
    }

public:
    // Tiles with IDs 14–22 are water and are considered blocking.
    bool isBlockedId(int id) const { return id >= 14 && id <= 22; }

    // Checks if the tile at (tx, ty) is blocking.
    // Out-of-bounds coordinates return false (handled by boundary logic elsewhere).
    bool isBlockedAt(int tx, int ty) const {
        int id = get(tx, ty);
        return (id >= 0) ? isBlockedId(id) : false;
    }

    // Tile size accessors
    int getTileW() const { return tileW; }
    int getTileH() const { return tileH; }

    // Map size in tiles
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    // Wrap behavior controls infinite map looping
    void setWrap(bool v) { wrap = v; }
    bool isWrap() const { return wrap; }

    // Constructor / destructor
    TileMap() { initCache(); }
    ~TileMap() {
        delete[] data;
        for (int i = 0; i < MAX_TILE_ID; ++i) {
            if (tileImg[i]) {
                tileImg[i]->~Image();
                delete tileImg[i];
                tileImg[i] = nullptr;
            }
        }
    }

    // Sets the folder containing tile images. Should end with a slash for convenience.
    void setImageFolder(const char* path) {
        int i = 0;
        for (; path[i] && i < 259; ++i) folder[i] = path[i];
        folder[i] = '\0';
    }

    // Loads map data from a "tiles.txt" file
    // Expected format includes:
    //   tileswide, tileshigh, tilewidth, tileheight
    // followed by layer data as a grid of integers
    bool load(const char* path) {
        std::ifstream f(path);
        if (!f) return false;

        std::string line;
        bool haveW = false, haveH = false, haveTW = false, haveTH = false;

        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line);
            std::string key; ss >> key;
            if (key == "tileswide") { ss >> width;  haveW = (width > 0); }
            else if (key == "tileshigh") { ss >> height; haveH = (height > 0); }
            else if (key == "tilewidth") { ss >> tileW;  haveTW = (tileW > 0); }
            else if (key == "tileheight") { ss >> tileH; haveTH = (tileH > 0); }
            else if (key == "layer") { break; }
        }

        if (!(haveW && haveH && haveTW && haveTH)) return false;

        delete[] data;
        data = new int[width * height];
        int idx = 0;

        while (idx < width * height && std::getline(f, line)) {
            if (line.empty()) continue;
            idx += parseInts(line, data + idx, width * height - idx);
        }
        return (idx == width * height);
    }

    // Retrieves a tile ID at (x, y).
    // In wrap mode, coordinates are wrapped using modular arithmetic.
    int get(int x, int y) const {
        if (!data) return -1;
        if (!wrap) {
            if (x < 0 || y < 0 || x >= width || y >= height) return -1;
            return data[y * width + x];
        }
        else {
            if (width <= 0 || height <= 0) return -1;
            int wx = x % width;  if (wx < 0) wx += width;
            int wy = y % height; if (wy < 0) wy += height;
            return data[wy * width + wx];
        }
    }

    // Renders visible portion of the map relative to the camera
    void draw(Window& window, float camX, float camY) {
        if (!data) return;
        int W = (int)window.getWidth(), H = (int)window.getHeight();

        int startTileX = (int)std::floor(camX / tileW) - 1;
        int startTileY = (int)std::floor(camY / tileH) - 1;
        int tilesX = W / tileW + 3;
        int tilesY = H / tileH + 3;

        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                int gx = startTileX + tx;
                int gy = startTileY + ty;
                int id = get(gx, gy);
                if (id < 0) continue;

                int sx = gx * tileW - (int)camX;
                int sy = gy * tileH - (int)camY;

                Image* img = getTileImage(id);
                if (img && (int)img->width == tileW && (int)img->height == tileH) {
                    blitImage(window, *img, sx, sy);
                }
                else {
                    // Fallback: fill with gray if image missing or mismatched size
                    unsigned char r = 40, g = 40, b = 40;
                    fillRect(window, sx, sy, tileW, tileH, r, g, b);
                }
            }
        }
    }

    // Returns total map size in pixels
    int getPixelWidth()  const { return width * tileW; }
    int getPixelHeight() const { return height * tileH; }
};
