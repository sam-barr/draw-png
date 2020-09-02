#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <png.h>

#include <xcb/xcb.h>

#define DRAW_PNG_VERSION "1.6.37"
#define PNG_SIG_LENGTH 8

#define SCREEN_NUMBER 0

#define draw_image_row_size(image) (sizeof(uint8_t) * 3 * (image)->width)

#define DEBUG_BOOL(b) printf("%s\n", (b) ? "true" : "false")

#define true 1
#define false 0

struct draw_png_reader {
        png_struct *png;
        png_info *info;
};

struct draw_png_writer {
        png_struct *png;
        png_info *info;
        FILE *file;
};

/*
 * 8-bit RGB image data
 */
struct draw_image {
        size_t width;
        size_t height;
        uint8_t *data;
};

struct draw_color {
        uint8_t r;
        uint8_t g;
        uint8_t b;
};

struct draw_pen {
        unsigned int radius;
        struct draw_color color;
};

/*
 * Reads the first 8 bytes and checks them against the magic png header
 * Should probably be followed with a call to "png_set_sig_bytes"
 */
int draw_check_if_png(FILE *file) {
        uint8_t bytes[PNG_SIG_LENGTH];
        fread(bytes, sizeof(uint8_t), PNG_SIG_LENGTH, file);
        return png_sig_cmp(bytes, 0, PNG_SIG_LENGTH) == 0;
}

/*
 * struct draw_png reading functions
 */

void draw_png_reader_init(struct draw_png_reader *reader, const char *file_name) {
        png_struct *png;
        png_info *info;
        FILE *file;

        png = png_create_read_struct(DRAW_PNG_VERSION, NULL, NULL, NULL);
        if (png == NULL) {
                fprintf(stderr, "Unable to create read struct\n");
                exit(EXIT_FAILURE);
        }

        info = png_create_info_struct(png);
        if (info == NULL) {
                fprintf(stderr, "Unable to create info struct\n");
                exit(EXIT_FAILURE);
        }

        file = fopen(file_name, "rb");
        if (file == NULL) {
                fprintf(stderr, "Unable to open file %s for reading\n", file_name);
                exit(EXIT_FAILURE);
        } else if (!draw_check_if_png(file)) {
                fprintf(stderr, "File %s is not a png file\n", file_name);
                exit(EXIT_FAILURE);
        }

        png_init_io(png, file);
        png_set_sig_bytes(png, PNG_SIG_LENGTH);
        png_read_png(
                png, info,
                PNG_TRANSFORM_STRIP_16 |
                PNG_TRANSFORM_PACKING |
                PNG_TRANSFORM_GRAY_TO_RGB |
                PNG_TRANSFORM_STRIP_ALPHA,
                NULL);

        reader->png = png;
        reader->info = info;
        fclose(file);
}

void draw_png_reader_destroy(struct draw_png_reader *reader) {
        png_destroy_read_struct(&reader->png, &reader->info, NULL);
}

/*
 * Load an image from a png reader
 */
void draw_png_reader_load_image(struct draw_png_reader *reader, struct draw_image *image) {
        size_t i, row_size;
        uint8_t **image_rows;

        image->width = png_get_image_width(reader->png, reader->info);
        image->height = png_get_image_height(reader->png, reader->info);
        row_size = draw_image_row_size(image);
        image->data = malloc(row_size * image->height);

        image_rows = png_get_rows(reader->png, reader->info);
        for (i = 0; i < image->height; i++)
                memcpy(image->data + i * row_size, image_rows[i], row_size);
}

/*
 * struct draw_png writing functions
 */

void draw_png_writer_init(struct draw_png_writer *writer, const char *file_name) {
        png_struct *png;
        png_info *info;
        FILE *file;

        png = png_create_write_struct(DRAW_PNG_VERSION, NULL, NULL, NULL);
        if (png == NULL) {
                fprintf(stderr, "Unable to create write struct\n");
                exit(EXIT_FAILURE);
        }

        info = png_create_info_struct(png);
        if (info == NULL) {
                fprintf(stderr, "Unable to create info struct\n");
                exit(EXIT_FAILURE);
        }

        file = fopen(file_name, "wb");
        if (file == NULL) {
                fprintf(stderr, "Unable to open file %s for writing\n", file_name);
                exit(EXIT_FAILURE);
        }

        png_init_io(png, file);
        png_set_compression_level(png, 2);
        writer->png = png;
        writer->info = info;
        writer->file = file;
}

void draw_png_writer_destroy(struct draw_png_writer *writer) {
        png_destroy_write_struct(&writer->png, &writer->info);
        fclose(writer->file);
}

void draw_png_writer_save_image(struct draw_png_writer *writer,
                const struct draw_image *image) {
        uint8_t **data;
        size_t row_size, i;

        row_size = draw_image_row_size(image);
        data = malloc(sizeof(uint8_t *) * image->height);
        for (i = 0; i < image->height; i++)
                data[i] = image->data + i * row_size;

        png_set_IHDR(
                writer->png,
                writer->info,
                image->width,
                image->height,
                8, /* bit depth of 8 */
                PNG_COLOR_TYPE_RGB,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
        png_set_rows(writer->png, writer->info, data);
        png_write_png(
                writer->png,
                writer->info,
                PNG_TRANSFORM_IDENTITY,
                NULL);
        free(data);
}

/*
 * struct draw_image functions
 */

void draw_image_destroy(struct draw_image *image) {
        free(image->data);
}

struct draw_color draw_image_get_pixel_color(const struct draw_image *image,
                size_t x, size_t y) {
        struct draw_color color;
        size_t row_size;

        row_size = draw_image_row_size(image);
        color.r = image->data[row_size * y + 3 * x];
        color.g = image->data[row_size * y + 3 * x + 1];
        color.b = image->data[row_size * y + 3 * x + 2];

        return color;
}

void draw_image_set_pixel_color(struct draw_image *image, struct draw_color color,
                size_t x, size_t y) {
        size_t row_size = draw_image_row_size(image);
        image->data[row_size * y + 3 * x]     = color.r;
        image->data[row_size * y + 3 * x + 1] = color.g;
        image->data[row_size * y + 3 * x + 2] = color.b;
}

/*
 * Convenience functions
 */

void draw_load_image(struct draw_image *image, const char *file_name) {
        struct draw_png_reader reader;
        draw_png_reader_init(&reader, file_name);
        draw_png_reader_load_image(&reader, image);
        draw_png_reader_destroy(&reader);
}

void draw_save_image(struct draw_image *image, const char *file_name) {
        struct draw_png_writer writer;
        draw_png_writer_init(&writer, file_name);
        draw_png_writer_save_image(&writer, image);
        draw_png_writer_destroy(&writer);
}

int main(void) {
        xcb_connection_t *connection;
        xcb_screen_t *screen;
        xcb_window_t window;
        int screen_number = SCREEN_NUMBER, mask, values[1];

        connection = xcb_connect(NULL, &screen_number);
        if (xcb_connection_has_error(connection)) {
                xcb_disconnect(connection);
                fprintf(stderr, "Unable to connect to X\n");
                return EXIT_FAILURE;
        }

        screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
        window = xcb_generate_id(connection);
        mask = XCB_CW_BACK_PIXEL;
        values[0] = screen->black_pixel;
        xcb_create_window(
                connection,
                XCB_COPY_FROM_PARENT, /* copy depth from parent */
                window,
                screen->root,
                0, 0, 1, 1, /* x,y, width, height */
                0, /* border width */
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                screen->root_visual,
                mask, values);
        xcb_map_window(connection, window);
        xcb_flush(connection);

        getchar();

        xcb_disconnect(connection);

        return EXIT_SUCCESS;
}
