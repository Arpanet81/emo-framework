// Copyright (c) 2011 emo-framework project
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the project nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, 
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
#include <android_native_app_glue.h>

#include "Constants.h"
#include "Engine.h"
#include "Runtime.h"
#include "Image.h"
#include "png.h"

extern emo::Engine* engine;

namespace emo {
    Image::Image() {
        this->hasData    = false;
        this->textureId  = 0;
        this->referenceCount = 0;
    }
    Image::~Image() {
        if (this->hasData) {
            free(this->data);
        }
    }
    void Image::genTextures() {
        glGenTextures(1, &this->textureId);
    }
}

/*
 * callback function to read png image
 */
static void png_asset_read(png_structp png_ptr, png_bytep data, png_uint_32 length) {
    AAsset* asset = (AAsset*)png_get_io_ptr(png_ptr);
    AAsset_read(asset, data, length);
}

bool loadPngSizeFromAsset(const char *fname, int *width, int *height) {
    AAssetManager* mgr = engine->app->activity->assetManager;
    if (mgr == NULL) {
    	engine->setLastError(ERR_ASSET_LOAD);
    	LOGE("loadPngSizeFromAsset: failed to load AAssetManager");
    	return false;
    }

    AAsset* asset = AAssetManager_open(mgr, fname, AASSET_MODE_UNKNOWN);
    if (asset == NULL) {
    	engine->setLastError(ERR_ASSET_OPEN);
    	LOGW("loadPngSizeFromAsset: failed to open asset");
        LOGW(fname);
    	return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL) {
        AAsset_close(asset);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        AAsset_close(asset);
        return false;
    }

    png_set_read_fn(png_ptr, (void *)asset, png_asset_read);

    unsigned int sig_read = 0;
    png_set_sig_bytes(png_ptr, sig_read);

    png_read_info(png_ptr, info_ptr);

    *width  = info_ptr->width;
    *height = info_ptr->height;

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    AAsset_close(asset);

    return true;
}

/* 
 * load png image from asset
 */
bool loadPngFromAsset(const char *fname, emo::Image* imageInfo) {
    AAssetManager* mgr = engine->app->activity->assetManager;
    if (mgr == NULL) {
    	engine->setLastError(ERR_ASSET_LOAD);
    	LOGE("loadPngFromAsset: failed to load AAssetManager");
    	return false;
    }

    AAsset* asset = AAssetManager_open(mgr, fname, AASSET_MODE_UNKNOWN);
    if (asset == NULL) {
    	engine->setLastError(ERR_ASSET_OPEN);
    	LOGW("loadPngFromAsset: failed to open asset");
        LOGW(fname);
    	return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL) {
        AAsset_close(asset);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        AAsset_close(asset);
        return false;
    }

    png_set_read_fn(png_ptr, (void *)asset, png_asset_read);

    unsigned int sig_read = 0;
    png_set_sig_bytes(png_ptr, sig_read);

    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, NULL);

    imageInfo->textureId = -1;
    imageInfo->filename = fname;
    imageInfo->width  = info_ptr->width;
    imageInfo->height = info_ptr->height;

    switch (info_ptr->color_type) {
        case PNG_COLOR_TYPE_RGBA:
            imageInfo->hasAlpha = true;
            break;
        case PNG_COLOR_TYPE_RGB:
            imageInfo->hasAlpha = false;
            break;
        default:
            LOGE("loadPngFromAsset: unsupported color type");
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            AAsset_close(asset);
            return false;
    }
    unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    imageInfo->data = (unsigned char*) malloc(row_bytes * imageInfo->height);

    png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

    for (int i = 0; i < imageInfo->height; i++) {
        memcpy(imageInfo->data+(row_bytes * (imageInfo->height-1-i)), row_pointers[i], row_bytes);
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    AAsset_close(asset);

    imageInfo->hasData = true;

    return true;
}
