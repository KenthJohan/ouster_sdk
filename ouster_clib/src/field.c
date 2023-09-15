#include "ouster_clib/field.h"
#include <platform/assert.h>
#include <string.h>
#include <stdlib.h>



void ouster_field_init(ouster_field_t fields[], int count, ouster_meta_t *meta)
{
	platform_assert_notnull(fields);
	platform_assert_notnull(meta);
	ouster_field_t *f = fields;
	for (int i = 0; i < count; ++i, f++)
	{
		ouster_extract_t * extract = meta->extract + f->quantity;
		f->data = calloc(extract->data_size, 1);
	}
}

void ouster_field_cpy(ouster_field_t dst[], ouster_field_t src[], int count, ouster_meta_t *meta)
{
	platform_assert_notnull(dst);
	platform_assert_notnull(src);
	platform_assert_notnull(meta);
	// memcpy(dst, src, sizeof(ouster_field_t) * count);
	for (int i = 0; i < count; ++i, ++dst, ++src)
	{
		ouster_extract_t * extract = meta->extract + src->quantity;
		memcpy(dst->data, src->data, extract->data_size);
	}
}

/*
template <typename T>
inline img_t<T> destagger(const Eigen::Ref<const img_t<T>>& img,
						  const std::vector<int>& pixel_shift_by_row,
						  bool inverse) {
	const size_t h = img.rows();
	const size_t w = img.cols();

	if (pixel_shift_by_row.size() != h)
		throw std::invalid_argument{"image height does not match shifts size"};

	img_t<T> destaggered{h, w};
	for (size_t u = 0; u < h; u++) {
		const std::ptrdiff_t offset =
			((inverse ? -1 : 1) * pixel_shift_by_row[u] + w) % w;

		destaggered.row(u).segment(offset, w - offset) =
			img.row(u).segment(0, w - offset);
		destaggered.row(u).segment(0, offset) =
			img.row(u).segment(w - offset, offset);
	}
	return destaggered;
}
*/

/*
https://static.ouster.dev/sdk-docs/reference/lidar-scan.html#staggering-and-destaggering
*/
void destagger(void *data, int cols, int rows, int depth, int rowsize, int pixel_shift_by_row[])
{
	platform_assert_notnull(data);
	platform_assert_notnull(pixel_shift_by_row);
	char *row = data;
	for (int irow = 0; irow < rows; ++irow, row += rowsize)
	{
		int offset = (pixel_shift_by_row[irow] + cols) % cols;
		memmove(row + depth * offset, row, depth * (cols - offset));
		memmove(row, row + depth * (cols - offset), depth * offset);
	}
}

void ouster_field_destagger(ouster_field_t fields[], int count, ouster_meta_t *meta)
{
	platform_assert_notnull(fields);
	platform_assert_notnull(meta);
	for (int i = 0; i < count; ++i, ++fields)
	{
		int rows = meta->pixels_per_column;
		int cols = meta->midw;
		ouster_extract_t * extract = meta->extract + fields->quantity;
		int depth = extract->depth;
		destagger(fields->data, cols, rows, depth, extract->rowsize, meta->pixel_shift_by_row);
	}
}

void ouster_field_apply_mask_u32(ouster_field_t *field, ouster_meta_t *meta)
{
	platform_assert_notnull(field);
	platform_assert_notnull(meta);
	ouster_extract_t * extract = meta->extract + field->quantity;
	uint32_t mask = extract->mask;
	int depth = extract->depth;
	if (mask == 0xFFFFFFFF)
	{
		return;
	}
	if (depth != 4)
	{
		return;
	}
	uint32_t *data32 = (uint32_t *)field->data;
	int rows = meta->pixels_per_column;
	int cols = meta->midw;
	for (int i = 0; i < rows * cols; ++i)
	{
		data32[i] &= mask;
	}
}

void ouster_field_zero(ouster_field_t fields[], int count, ouster_meta_t *meta)
{
	platform_assert_notnull(fields);
	platform_assert_notnull(meta);
	for (int i = 0; i < count; ++i, ++fields)
	{
		ouster_extract_t * extract = meta->extract + fields->quantity;
		memset(fields->data, 0, extract->data_size);
	}
}
