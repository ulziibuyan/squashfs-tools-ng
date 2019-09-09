/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr.h - This file is part of libsquashfs
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef SQFS_XATTR_H
#define SQFS_XATTR_H

#include "sqfs/predef.h"

/**
 * @file xattr.h
 *
 * @brief Contains on-disk data structures for storing extended attributes and
 *        declarations for the @ref sqfs_xattr_reader_t.
 */

/**
 * @enum E_SQFS_XATTR_TYPE
 *
 * Used by @ref sqfs_xattr_entry_t to encodes the xattr prefix.
 */
typedef enum {
	SQFS_XATTR_USER = 0,
	SQFS_XATTR_TRUSTED = 1,
	SQFS_XATTR_SECURITY = 2,

	SQFS_XATTR_FLAG_OOL = 0x100,
	SQFS_XATTR_PREFIX_MASK = 0xFF,
} E_SQFS_XATTR_TYPE;

/**
 * @struct sqfs_xattr_entry_t
 *
 * @brief On-disk data structure that holds a single xattr key
 *
 * See @ref sqfs_xattr_reader_t for an overview how SquashFS stores extended
 * attributes on disk.
 */
struct sqfs_xattr_entry_t {
	/**
	 * @brief Encodes the prefix of the key
	 *
	 * A @ref E_SQFS_XATTR_TYPE value. If the @ref SQFS_XATTR_FLAG_OOL is
	 * set, the value that follows is not actually a string but a 64 bit
	 * reference to the location where the value is actually stored.
	 */
	uint16_t type;

	/**
	 * @brief The size in bytes of the suffix string that follows
	 */
	uint16_t size;
	uint8_t key[];
};

/**
 * @struct sqfs_xattr_value_t
 *
 * @brief On-disk data structure that holds a single xattr value
 *
 * See @ref sqfs_xattr_reader_t for an overview how SquashFS stores extended
 * attributes on disk.
 */
struct sqfs_xattr_value_t {
	/**
	 * @brief The exact size in bytes of the value that follows
	 */
	uint32_t size;
	uint8_t value[];
};

/**
 * @struct sqfs_xattr_id_t
 *
 * @brief On-disk data structure that describes a set of key-value pairs
 *
 * See @ref sqfs_xattr_reader_t for an overview how SquashFS stores extended
 * attributes on disk.
 */
struct sqfs_xattr_id_t {
	/**
	 * @brief Location of the first key-value pair
	 *
	 * This is a reference, i.e. the bits 16 to 48 hold an offset that is
	 * added to xattr_table_start from @ref sqfs_xattr_id_table_t to get
	 * the location of a meta data block that contains the first key-value
	 * pair. The lower 16 bits store an offset into the uncompressed meta
	 * data block.
	 */
	uint64_t xattr;

	/**
	 * @brief Number of consecutive key-value pairs
	 */
	uint32_t count;

	/**
	 * @brief Total size of the uncompressed key-value pairs in bytes,
	 *        including data structures used to encode them.
	 */
	uint32_t size;
};

/**
 * @struct sqfs_xattr_id_table_t
 *
 * @brief On-disk data structure that the super block points to
 *
 * Indicates the locations of the xattr key-value pairs and descriptor array.
 * See @ref sqfs_xattr_reader_t for an overview how SquashFS stores extended
 * attributes on disk.
 */
struct sqfs_xattr_id_table_t {
	/**
	 * @brief The location of the first meta data block holding the key
	 *        value pairs.
	 */
	uint64_t xattr_table_start;

	/**
	 * @brief The total number of descriptors (@ref sqfs_xattr_id_t)
	 */
	uint32_t xattr_ids;

	/**
	 * @brief Unused, alwayas set this to 0 when writing!
	 */
	uint32_t unused;

	/**
	 * @brief Holds the locations of the meta data blocks that contain the
	 *        @ref sqfs_xattr_id_t descriptor array.
	 */
	uint64_t locations[];
};

/**
 * @struct sqfs_xattr_reader_t
 *
 * @brief Abstracts read access to extended attributes in a SquashFS filesystem
 *
 * SquashFS stores extended attributes using multiple levels of indirection.
 * First of all, the key-value pairs of each inode (that has extended
 * attributes) are deduplicated and stored consecutively in meta data blocks.
 * Furthermore, a value can be stored out-of-band, i.e. it holds a reference to
 * another location from which the value has to be read.
 *
 * For each unique set of key-value pairs, a descriptor object is generated
 * that holds the location of the first pair, the number of pairs and the total
 * size used on disk. The array of descriptor objects is stored in multiple
 * meta data blocks. Each inode that has extended attributes holds a 32 bit
 * index into the descriptor array.
 *
 * The third layer of indirection is yet another table that points to the
 * locations of the previous two tables. Its location is in turn stored in
 * the super block.
 *
 * The sqfs_xattr_reader_t data structure takes care of the low level details
 * of loading and parsing the data.
 *
 * After creating an instance using @ref sqfs_xattr_reader_create, simply call
 * @ref sqfs_xattr_reader_load_locations to load and parse all of the location
 * tables. Then use @ref sqfs_xattr_reader_get_desc to resolve a 32 bit index
 * from an inode to a descriptor structure. Use @ref sqfs_xattr_reader_seek_kv
 * to point the reader to the start of the key-value pairs and the call
 * @ref sqfs_xattr_reader_read_key and @ref sqfs_xattr_reader_read_value
 * consecutively to read and decode each key-value pair.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resolve an xattr identifier to the coresponding prefix
 *
 * Like many file systems, SquashFS stores xattrs be cutting off the common
 * prefix of the key string and storing an enumerator instead to save memory.
 *
 * This function takes an @ref E_SQFS_XATTR_TYPE identifier and returns the
 * coresponding prefix string, including the '.' at the end that seperates
 * the prefix from the rest of the key.
 */
SQFS_API const char *sqfs_get_xattr_prefix(E_SQFS_XATTR_TYPE id);

/**
 * @brief Resolve an xattr prefix into an identifier
 *
 * Like many file systems, SquashFS stores xattrs be cutting off the common
 * prefix of the key string and storing an enumerator instead to save memory.
 *
 * This function takes a key and finds the enumerator value that represents
 * its prefix.
 *
 * This function will return a failure value to indicate that the given prefix
 * isn't supported. Another function called @ref sqfs_has_xattr is provided to
 * explicitly check if a prefix is supported.
 *
 * @return On success an @ref E_SQFS_XATTR_TYPE, -1 if it isn't supported.
 */
SQFS_API int sqfs_get_xattr_prefix_id(const char *key);

/**
 * @brief Check if a given xattr key can actually be encoded in SquashFS
 *
 * Like many file systems, SquashFS stores xattrs be cutting off the common
 * prefix of the key string and storing an enumerator instead to save memory.
 *
 * However, this means if new prefixes are introduced, they are not immediately
 * supported since SquashFS may not have an enumerator defined for them.
 *
 * This function checks if the @ref sqfs_get_xattr_prefix_id function can
 * translate the prefix of the given key into a coresponding enumerator.
 *
 * If it returns false, this means either that SquashFS doesn't support this
 * prefix, or it has recently been added but the version of libsquashfs you
 * are using doesn't support it.
 */
SQFS_API bool sqfs_has_xattr(const char *key);

/**
 * @brief Load the locations of the xattr meta data blocks into memory
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This function must be called explicitly after an xattr reader has been
 * created to load the actual location table from disk.
 *
 * @return Zero on success, a negative @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_reader_load_locations(sqfs_xattr_reader_t *xr);

/**
 * @brief Destroy an xattr reader and free all memory used by it
 *
 * @memberof sqfs_xattr_reader_t
 *
 * @param xr A pointer to an xattr reader instance
 */
SQFS_API void sqfs_xattr_reader_destroy(sqfs_xattr_reader_t *xr);

/**
 * @brief Create an xattr reader
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This function creates an object that abstracts away read only access to
 * the extended attributes in a SquashFS filesystem.
 *
 * After creating a reader and before using it, call
 * @ref sqfs_xattr_reader_load_locations to load and parse the location
 * information required to look up xattr key-value pairs.
 *
 * All pointers passed to this function are stored internally for later use.
 * Do not destroy any of the pointed to objects before destroying the xattr
 * reader.
 *
 * @param file A pointer to a file object that contains the SquashFS filesystem
 * @param super A pointer to the SquashFS super block required to find the
 *              location tables.
 * @param cmp A pointer to a compressor used to uncompress the loaded meta data
 *            blocks.
 *
 * @return A pointer to a new xattr reader instance on success, NULL on
 *         allocation failure.
 */
SQFS_API sqfs_xattr_reader_t *sqfs_xattr_reader_create(sqfs_file_t *file,
						       sqfs_super_t *super,
						       sqfs_compressor_t *cmp);

/**
 * @brief Resolve an xattr index from an inode to an xattr description
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This function takes an xattr index from an extended inode type and resolves
 * it to a descriptor that points to location of the key-value pairs and
 * indicates how many key-value pairs to read from there.
 *
 * @param xr A pointer to an xattr reader instance
 * @param idx The xattr index to resolve
 * @param desc Used to return the description
 *
 * @return Zero on success, a negative @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_reader_get_desc(sqfs_xattr_reader_t *xr, uint32_t idx,
					sqfs_xattr_id_t *desc);

/**
 * @brief Resolve an xattr index from an inode to an xattr description
 *
 * @memberof sqfs_xattr_reader_t
 *
 * This function takes an xattr descriptor object and seeks to the meta data
 * block containing the key-value pairs. The individual pairs can then be read
 * using consecutive calls to @ref sqfs_xattr_reader_read_key and
 * @ref sqfs_xattr_reader_read_value.
 *
 * @param xr A pointer to an xattr reader instance
 * @param desc The descriptor holding the location of the key-value pairs
 *
 * @return Zero on success, a negative @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_xattr_reader_seek_kv(sqfs_xattr_reader_t *xr,
				       const sqfs_xattr_id_t *desc);

/**
 * @brief Read the next xattr key
 *
 * @memberof sqfs_xattr_reader_t
 *
 * After setting the start position using @ref sqfs_xattr_reader_seek_kv, this
 * function reads and decodes an xattr key and advances the internal position
 * indicator to the location after the key. The value can then be read using
 * using @ref sqfs_xattr_reader_read_value. After reading the value, the next
 * key can be read by calling this function again.
 *
 * @param xr A pointer to an xattr reader instance
 * @param key_out Used to return the decoded key. The underlying memory can be
 *                released using a single free() call.
 *
 * @return Zero on success, a negative @ref E_SQFS_ERROR value on failure.
 */
SQFS_API
int sqfs_xattr_reader_read_key(sqfs_xattr_reader_t *xr,
			       sqfs_xattr_entry_t **key_out);

/**
 * @brief Read the xattr value belonging to the last read key
 *
 * @memberof sqfs_xattr_reader_t
 *
 * After calling @ref sqfs_xattr_reader_read_key, this function can read and
 * decode the asociated value. The internal location indicator is then advanced
 * past the key to the next value, so @ref sqfs_xattr_reader_read_key can be
 * called again to read the next key.
 *
 * @param xr A pointer to an xattr reader instance.
 * @param key A pointer to the decoded key object.
 * @param val_out Used to return the decoded value. The underlying memory can
 *                be released using a single free() call.
 *
 * @return Zero on success, a negative @ref E_SQFS_ERROR value on failure.
 */
SQFS_API
int sqfs_xattr_reader_read_value(sqfs_xattr_reader_t *xr,
				 const sqfs_xattr_entry_t *key,
				 sqfs_xattr_value_t **val_out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_XATTR_H */
