#include <cstring>
#include <string>
#include <memory>
#include <sys/stat.h>

// BMP - explicitly use C linkage
extern "C" {
#include "libnsbmp.h"
}

// GIF
#include <gif_lib.h>

// JPEG
#include <turbojpeg.h>

// STB - suppress unused function warnings from header-only library
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_BMP
#define STBI_NO_HDR
#define STBI_NO_JPEG
#define STBI_NO_PIC
#define STBI_NO_PNG
#define STBI_ONLY_GIF
#define STBI_ONLY_PNM
#define STBI_ONLY_PSD
#define STBI_ONLY_TGA
#include "stb_image.h"
#pragma GCC diagnostic pop

// PNG
#include <png.h>

// WEBP
#include <webp/decode.h>

#include <switch.h>

#include "fs.hpp"
#include "gui.hpp"
#include "imgui_impl_switch.hpp"
#include "log.hpp"
#include "services.hpp"
#include "textures.hpp"
#include "windows.hpp"

#define BYTES_PER_PIXEL 4

#define MAX_IMAGE_BYTES (48 * 1024 * 1024)


namespace BMP {
    static void *bitmap_create(int width, int height, [[maybe_unused]] unsigned int state) {
        /* ensure a stupidly large (>50Megs or so) bitmap is not created */
        if ((static_cast<long long>(width) * static_cast<long long>(height)) > (MAX_IMAGE_BYTES/BYTES_PER_PIXEL))
            return nullptr;
            
        return std::calloc(width * height, BYTES_PER_PIXEL);
    }
    
    static unsigned char *bitmap_get_buffer(void *bitmap) {
        assert(bitmap);
        return static_cast<unsigned char *>(bitmap);
    }
    
    static void bitmap_destroy(void *bitmap) {
        assert(bitmap);
        std::free(bitmap);
    }
}

namespace Textures {
    typedef enum ImageType {
        ImageTypeBMP,
        ImageTypeGIF,
        ImageTypeJPEG,
        ImageTypePNG,
        ImageTypeWEBP,
        ImageTypeOther
    } ImageType;
    
    static bool ReadFile(const std::string &path, std::unique_ptr<unsigned char[]> &buffer, std::size_t &size) {
        FILE *file = fopen(path.c_str(), "rb");
        if (!file) {
            Log::Error("Textures::ReadFile (%s) failed to open file.\n", path.c_str());
            return false;
        }

        struct stat file_stat = { 0 };
        if (stat(path.c_str(), std::addressof(file_stat)) != 0) {
            Log::Error("Textures::ReadFile (%s) failed to get file size.\n", path.c_str());
            fclose(file);
            return false;
        }

        size = file_stat.st_size;
        buffer = std::make_unique<unsigned char[]>(size + 1);
        std::size_t bytes_read = fread(buffer.get(), sizeof(unsigned char), size, file);

        if (bytes_read != size) {
            Log::Error("Textures::ReadFile (%s) failed to read file.\n", path.c_str());
            fclose(file);
            return false;
        }

        fclose(file);
        return true;
    }

    static bool Create(unsigned char *data, GLint format, Tex &texture) {
        glGenTextures(1, std::addressof(texture.id));
        glBindTexture(GL_TEXTURE_2D, texture.id);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same
        
        // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
        glTexImage2D(GL_TEXTURE_2D, 0, format, texture.width, texture.height, 0, format, GL_UNSIGNED_BYTE, data);
        return true;
    }
    
    static bool LoadImagePNG(const std::string &path, Tex &texture) {
        bool ret = false;
        png_image image;
        std::memset(std::addressof(image), 0, (sizeof image));
        image.version = PNG_IMAGE_VERSION;

        if (png_image_begin_read_from_file(std::addressof(image), path.c_str()) != 0) {
            image.format = PNG_FORMAT_RGBA;
            auto buffer = std::make_unique<png_byte[]>(PNG_IMAGE_SIZE(image));

            if (buffer && png_image_finish_read(std::addressof(image), nullptr, buffer.get(), 0, nullptr) != 0) {
                texture.width = image.width;
                texture.height = image.height;
                ret = Textures::Create(buffer.get(), GL_RGBA, texture);
                png_image_free(std::addressof(image));
            }
            else {
                if (!buffer)
                    png_image_free(std::addressof(image));
            }
        }

        return ret;
    }
    
    static bool LoadImageBMP(const std::unique_ptr<unsigned char[]> &data, std::size_t size, Tex &texture) {
        bmp_bitmap_callback_vt bitmap_callbacks = {
            BMP::bitmap_create,
            BMP::bitmap_destroy,
            BMP::bitmap_get_buffer
        };
        
        bmp_result code = BMP_OK;
        bmp_image bmp;
        bmp_create(std::addressof(bmp), std::addressof(bitmap_callbacks));
        
        code = bmp_analyse(std::addressof(bmp), size, data.get());
        if (code != BMP_OK) {
            bmp_finalise(std::addressof(bmp));
            return false;
        }
        
        code = bmp_decode(std::addressof(bmp));
        if (code != BMP_OK) {
            if ((code != BMP_INSUFFICIENT_DATA) && (code != BMP_DATA_ERROR)) {
                bmp_finalise(std::addressof(bmp));
                return false;
            }
            
            /* skip if the decoded image would be ridiculously large */
            if ((bmp.width * bmp.height) > 200000) {
                bmp_finalise(std::addressof(bmp));
                return false;
            }
        }
        
        texture.width = bmp.width;
        texture.height = bmp.height;
        bool ret = Textures::Create(static_cast<unsigned char *>(bmp.bitmap), GL_RGBA, texture);
        bmp_finalise(std::addressof(bmp));
        return ret;
    }

    static bool LoadImageGIF(const std::string &path, std::vector<Tex> &textures) {
        bool ret = false;
        int error = 0;
        GifFileType *gif = DGifOpenFileName(path.c_str(), std::addressof(error));

        if (!gif) {
            Log::Error("DGifOpenFileName failed: %d\n", error);
            return ret;
        }

        if (DGifSlurp(gif) != GIF_OK) {
            Log::Error("DGifSlurp failed: %d\n", gif->Error);
            return ret;
        }

        if (gif->ImageCount <= 0) {
            Log::Error("Gif does not contain any images.\n");
            return ret;
        }
        
        textures.resize(gif->ImageCount);
        
        // seiken's example code from:
        // https://forums.somethingawful.com/showthread.php?threadid=2773485&userid=0&perpage=40&pagenumber=487#post465199820
        textures[0].width = gif->SWidth;
        textures[0].height = gif->SHeight;
        std::unique_ptr<u32[]> pixels(new u32[textures[0].width * textures[0].height]);
        
        for (int i = 0; i < textures[0].width * textures[0].height; ++i)
            pixels[i] = gif->SBackGroundColor;
            
        for (int i = 0; i < gif->ImageCount; ++i) {
            const SavedImage &frame = gif->SavedImages[i];
            bool transparency = false;
            unsigned char transparency_byte = 0;
            
            // Delay time in hundredths of a second.
            int delay_time = 1;
            for (int j = 0; j < frame.ExtensionBlockCount; ++j) {
                const ExtensionBlock &block = frame.ExtensionBlocks[j];
                
                if (block.Function != GRAPHICS_EXT_FUNC_CODE)
                    continue;
                    
                // Here's the metadata for this frame.
                char dispose = (block.Bytes[0] >> 2) & 7;
                transparency = block.Bytes[0] & 1;
                delay_time = block.Bytes[1] + (block.Bytes[2] << 8);
                transparency_byte = block.Bytes[3];
                
                if (dispose == 2) {
                    // Clear the canvas.
                    for (int k = 0; k < textures[0].width * textures[0].height; ++k)
                        pixels[k] = gif->SBackGroundColor;
                }
            }
            
            // Colour map for this frame.
            ColorMapObject *map = frame.ImageDesc.ColorMap ? frame.ImageDesc.ColorMap : gif->SColorMap;
            
            // Region this frame draws to.
            int fw = frame.ImageDesc.Width;
            int fh = frame.ImageDesc.Height;
            int fl = frame.ImageDesc.Left;
            int ft = frame.ImageDesc.Top;
            
            for (int y = 0; y < std::min(textures[0].height, fh); ++y) {
                for (int x = 0; x < std::min(textures[0].width, fw); ++x) {
                    unsigned char byte = frame.RasterBits[x + y * fw];

                    // Transparent pixel.
                    if (transparency && byte == transparency_byte)
                        continue;
                        
                    // Draw to canvas.
                    const GifColorType &c = map->Colors[byte];
                    pixels[fl + x + (ft + y) * textures[0].width] = c.Red | (c.Green << 8) | (c.Blue << 16) | (0xff << 24);
                }
            }

            textures[i].delay = delay_time * 10000000;
            textures[i].width = textures[0].width;
            textures[i].height = textures[0].height;
            
            // Here's the actual frame, pixels.get() is now a pointer to the 32-bit RGBA
            // data for this frame you might expect.
            ret = Textures::Create(reinterpret_cast<unsigned char*>(pixels.get()), GL_RGBA, textures[i]);
        }
        
        if (DGifCloseFile(gif, std::addressof(error)) != GIF_OK) {
            Log::Error("DGifCloseFile failed: %d\n", error);
            return false;
        }

        return true;
    }
    
    static bool LoadImageJPEG(const std::unique_ptr<unsigned char[]> &data, std::size_t size, Tex &texture) {
        tjhandle jpeg = tjInitDecompress();
        int jpegsubsamp = 0;
        tjDecompressHeader2(jpeg, data.get(), size, std::addressof(texture.width), std::addressof(texture.height), std::addressof(jpegsubsamp));
        auto buffer = std::make_unique<unsigned char[]>(texture.width * texture.height * 4);
        tjDecompress2(jpeg, data.get(), size, buffer.get(), texture.width, 0, texture.height, TJPF_RGBA, TJFLAG_FASTDCT);
        bool ret = Textures::Create(buffer.get(), GL_RGBA, texture);
        tjDestroy(jpeg);
        return ret;
    }
    
    static bool LoadImageOther(const std::string &path, Tex &texture) {
        unsigned char *image = stbi_load(path.c_str(), std::addressof(texture.width), std::addressof(texture.height), nullptr, STBI_rgb_alpha);
        bool ret = Textures::Create(image, GL_RGBA, texture);
        return ret;
    }

    static bool LoadImageWEBP(const std::unique_ptr<unsigned char[]> &data, std::size_t size, Tex &texture) {
        // WebPDecodeRGBA allocates and returns new memory that must be freed with WebPFree
        unsigned char *decoded = WebPDecodeRGBA(data.get(), size, std::addressof(texture.width), std::addressof(texture.height));
        bool ret = Textures::Create(decoded, GL_RGBA, texture);
        WebPFree(decoded);
        return ret;
    }

    ImageType GetImageType(const std::string &filename) {
        std::string ext = FS::GetFileExt(filename);
        
        if (!ext.compare(".BMP"))
            return ImageTypeBMP;
        else if (!ext.compare(".GIF"))
            return ImageTypeGIF;
        else if ((!ext.compare(".JPG")) || (!ext.compare(".JPEG")))
            return ImageTypeJPEG;
        else if (!ext.compare(".PNG"))
            return ImageTypePNG;
        else if (!ext.compare(".WEBP"))
            return ImageTypeWEBP;
        
        return ImageTypeOther;
    }

    bool LoadImageFile(const std::string &path, std::vector<Tex> &textures) {
        bool ret = false;

        // Resize to 1 initially. If the file is a GIF it will be resized accordingly.
        textures.resize(1);

        ImageType type = Textures::GetImageType(path);

        if (type == ImageTypeGIF)
            ret = Textures::LoadImageGIF(path, textures);
        else if (type == ImageTypePNG)
            ret = Textures::LoadImagePNG(path, textures[0]);
        else if (type == ImageTypeOther)
            ret = Textures::LoadImageOther(path, textures[0]);
        else {
            std::unique_ptr<unsigned char[]> data;
            std::size_t size = 0;
            
            if (!Textures::ReadFile(path, data, size)) {
                return ret;
            }

            switch(type) {
                case ImageTypeBMP:
                    ret = Textures::LoadImageBMP(data, size, textures[0]);
                    break;
                    
                case ImageTypeJPEG:
                    ret = Textures::LoadImageJPEG(data, size, textures[0]);
                    break;
                    
                case ImageTypeWEBP:
                    ret = Textures::LoadImageWEBP(data, size, textures[0]);
                    break;
                    
                default:
                    break;
            }
        }
        return ret;
    }
    
    void Init(App &app) {
        const int num_icons = 8;

        const std::string paths[num_icons] {
            "romfs:/file.png",
            "romfs:/archive.png",
            "romfs:/image.png",
            "romfs:/text.png",
            "romfs:/file0x.png",  // Binary/Hex icon
            "romfs:/switch.png",  // Nintendo Switch files
            "romfs:/switch.png",  // Nintendo Switch installable (NSP) - same icon
            "romfs:/switch.png"   // Nintendo Switch NRO homebrew - same icon
        };

        // Load all icons - don't use IM_ASSERT for calls with side effects
        // (IM_ASSERT can be disabled in release builds, skipping the actual load!)
        Textures::LoadImagePNG("romfs:/folder.png", app.textures.folder_icon);
        Textures::LoadImagePNG("romfs:/check.png", app.textures.check_icon);
        Textures::LoadImagePNG("romfs:/uncheck.png", app.textures.uncheck_icon);
        Textures::LoadImagePNG("romfs:/partcheck.png", app.textures.partcheck_icon);
        
        // Try to load drive icon, fall back to folder icon if not available
        if (!Textures::LoadImagePNG("romfs:/drive.png", app.textures.drive_icon)) {
            app.textures.drive_icon = app.textures.folder_icon;  // Use folder icon as fallback
        }
        
        Textures::LoadImagePNG("romfs:/settings.png", app.textures.settings_icon);
        
        app.textures.file_icons.resize(num_icons);

        for (int i = 0; i < num_icons; i++) {
            Textures::LoadImagePNG(paths[i], app.textures.file_icons[i]);
        }
    }
    
    void Free(Tex &texture) {
        glDeleteTextures(1, std::addressof(texture.id));
    }
    
    void Exit(App &app) {
        // Clean up any deferred texture deletions first
        ImageViewer::CleanupDeferredDeletions();
        
        for (unsigned int i = 0; i < app.textures.file_icons.size(); i++)
            Textures::Free(app.textures.file_icons[i]);

        Textures::Free(app.textures.uncheck_icon);
        Textures::Free(app.textures.check_icon);
        Textures::Free(app.textures.partcheck_icon);
        Textures::Free(app.textures.folder_icon);
        Textures::Free(app.textures.settings_icon);
        // Only free drive_icon if it's not the same as folder_icon (i.e., was loaded separately)
        if (app.textures.drive_icon.id != app.textures.folder_icon.id)
            Textures::Free(app.textures.drive_icon);

        for (unsigned int i = 0; i < app.window.textures.size(); i++)
            Textures::Free(app.window.textures[i]);
        
        // Free pre-loaded image textures
        for (unsigned int i = 0; i < app.window.textures_prev.size(); i++)
            Textures::Free(app.window.textures_prev[i]);
        for (unsigned int i = 0; i < app.window.textures_next.size(); i++)
            Textures::Free(app.window.textures_next[i]);
    }
}
