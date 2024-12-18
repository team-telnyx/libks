/*
* Copyright (c) 2007,2008 Mij <mij@bitchx.it>
*
* Permission to use, copy, modify, and distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


/*
* Original SimCList library. See http://mij.oltrelinux.com/devel/simclist
*/


#ifndef SIMCLIST_H
#define SIMCLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libks/ks.h"
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>

#ifndef SIMCLIST_NO_DUMPRESTORE
#   ifndef _WIN32
#       include <sys/time.h>    /* list_dump_info_t's struct timeval */
#   else
#       include <time.h>
#   endif
#endif


	/* Be friend of both C90 and C99 compilers */
#if __STDC_VERSION__ >= 199901L
  /* "inline" is a keyword */
#else
# define INLINE
#endif

#ifndef INLINE
# if __GNUC__ && !__GNUC_STDC_INLINE__
#  define INLINE static inline
# else
#  define INLINE inline
# endif
#endif

#if defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
# define restrict __restrict
#elif defined(_MSC_VER) && _MSC_VER >= 1400
# define restrict __restrict
#else
# define restrict
#endif


	/**
	* Type representing list hashes.
	*
	* This is a signed integer value.
	*/
	typedef int32_t ks_list_hash_t;

#ifndef SIMCLIST_NO_DUMPRESTORE
	typedef struct {
		uint16_t version;           /* dump version */
		struct timeval timestamp;   /* when the list has been dumped, seconds since UNIX epoch */
		uint32_t list_size;
		uint32_t list_numels;
		ks_list_hash_t list_hash;      /* hash of the list when dumped, or 0 if invalid */
		uint32_t dumpsize;
		int consistent;             /* 1 if the dump is verified complete/consistent; 0 otherwise */
	} ks_list_dump_info_t;
#endif

	/**
	* a comparator of elements.
	*
	* A comparator of elements is a function that:
	*      -# receives two references to elements a and b
	*      -# returns {<0, 0, >0} if (a > b), (a == b), (a < b) respectively
	*
	* It is responsability of the function to handle possible NULL values.
	*/
	typedef int(*element_comparator)(const void *a, const void *b);

	/**
	* a seeker of elements.
	*
	* An element seeker is a function that:
	*      -# receives a reference to an element el
	*      -# receives a reference to some indicator data
	*      -# returns non-0 if the element matches the indicator, 0 otherwise
	*
	* It is responsability of the function to handle possible NULL values in any
	* argument.
	*/
	typedef int(*element_seeker)(const void *el, const void *indicator);

	/**
	* an element lenght meter.
	*
	* An element meter is a function that:
	*      -# receives the reference to an element el
	*      -# returns its size in bytes
	*
	* It is responsability of the function to handle possible NULL values.
	*/
	typedef ks_size_t(*element_meter)(const void *el);

	/**
	* a function computing the hash of elements.
	*
	* An hash computing function is a function that:
	*      -# receives the reference to an element el
	*      -# returns a hash value for el
	*
	* It is responsability of the function to handle possible NULL values.
	*/
	typedef ks_list_hash_t(*element_hash_computer)(const void *el);

	/**
	* a function for serializing an element.
	*
	* A serializer function is one that gets a reference to an element,
	* and returns a reference to a buffer that contains its serialization
	* along with the length of this buffer.
	* It is responsability of the function to handle possible NULL values,
	* returning a NULL buffer and a 0 buffer length.
	*
	* These functions have 3 goals:
	*  -# "freeze" and "flatten" the memory representation of the element
	*  -# provide a portable (wrt byte order, or type size) representation of the element, if the dump can be used on different sw/hw combinations
	*  -# possibly extract a compressed representation of the element
	*
	* @param el                reference to the element data
	* @param serialize_buffer  reference to fill with the length of the buffer
	* @return                  reference to the buffer with the serialized data
	*/
	typedef void *(*element_serializer)(const void *restrict el, uint32_t *restrict serializ_len);

	/**
	* a function for un-serializing an element.
	*
	* An unserializer function accomplishes the inverse operation of the
	* serializer function.  An unserializer function is one that gets a
	* serialized representation of an element and turns it backe to the original
	* element. The serialized representation is passed as a reference to a buffer
	* with its data, and the function allocates and returns the buffer containing
	* the original element, and it sets the length of this buffer into the
	* integer passed by reference.
	*
	* @param data              reference to the buffer with the serialized representation of the element
	* @param data_len          reference to the location where to store the length of the data in the buffer returned
	* @return                  reference to a buffer with the original, unserialized representation of the element
	*/
	typedef void *(*element_unserializer)(const void *restrict data, uint32_t *restrict data_len);

	/* [private-use] list entry -- olds actual user datum */
	struct ks_list_entry_s {
		void *data;

		/* doubly-linked list service references */
		struct ks_list_entry_s *next;
		struct ks_list_entry_s *prev;
	};

	/* [private-use] list attributes */
	struct ks_list_attributes_s {
		/* user-set routine for comparing list elements */
		element_comparator comparator;
		/* user-set routing for seeking elements */
		element_seeker seeker;
		/* user-set routine for determining the length of an element */
		element_meter meter;
		int copy_data;
		/* user-set routine for computing the hash of an element */
		element_hash_computer hasher;
		/* user-set routine for serializing an element */
		element_serializer serializer;
		/* user-set routine for unserializing an element */
		element_unserializer unserializer;
	};

	/** list object */
	typedef struct {
		ks_rwl_t *lock;

		struct ks_list_entry_s *head_sentinel;
		struct ks_list_entry_s *tail_sentinel;
		struct ks_list_entry_s *mid;

		unsigned int numels;

		/* array of spare elements */
		struct ks_list_entry_s **spareels;
		unsigned int spareelsnum;

#ifdef SIMCLIST_WITH_THREADS
		/* how many threads are currently running */
		unsigned int threadcount;
#endif

		/* service variables for list iteration */
		int iter_active;
		unsigned int iter_pos;
		struct ks_list_entry_s *iter_curentry;

		/* list attributes */
		struct ks_list_attributes_s attrs;
	} ks_list_t;

	/**
	* initialize a list object for use.
	*
	* @param list  must point to a user-provided memory location for a pointer to allocate
	* @param pool  pool for lifecycle and auto cleanup
	* @return      KS_STATUS_SUCCESS for success.
	*/
	KS_DECLARE(ks_status_t) ks_list_create(ks_list_t **list, ks_pool_t *pool);

	/**
	* completely remove the list from memory.
	*
	* This function is the inverse of ks_list_create(). It is meant to be called when
	* the list is no longer going to be used. Elements and possible memory taken
	* for internal use are freed.
	*
	* @param list  pointer to pointer of list to destroy
	* @return      KS_STATUS_SUCCESS for success.
	*/
	KS_DECLARE(ks_status_t) ks_list_destroy(ks_list_t **list);

	/**
	* set the comparator function for list elements.
	*
	* Comparator functions are used for searching and sorting. If NULL is passed
	* as reference to the function, the comparator is disabled.
	*
	* @param l     list to operate
	* @param comparator_fun    pointer to the actual comparator function
	* @return      0 if the attribute was successfully set; -1 otherwise
	*
	* @see element_comparator()
	*/
	KS_DECLARE(int) ks_list_attributes_comparator(ks_list_t *restrict l, element_comparator comparator_fun);

	/**
	* set a seeker function for list elements.
	*
	* Seeker functions are used for finding elements. If NULL is passed as reference
	* to the function, the seeker is disabled.
	*
	* @param l     list to operate
	* @param seeker_fun    pointer to the actual seeker function
	* @return      0 if the attribute was successfully set; -1 otherwise
	*
	* @see element_seeker()
	*/
	KS_DECLARE(int) ks_list_attributes_seeker(ks_list_t *restrict l, element_seeker seeker_fun);

	/**
	* require to free element data when list entry is removed (default: don't free).
	*
	* [ advanced preference ]
	*
	* By default, when an element is removed from the list, it disappears from
	* the list by its actual data is not free()d. With this option, every
	* deletion causes element data to be freed.
	*
	* It is responsability of this function to correctly handle NULL values, if
	* NULL elements are inserted into the list.
	*
	* @param l             list to operate
	* @param metric_fun    pointer to the actual metric function
	* @param copy_data     0: do not free element data (default); non-0: do free
	* @return          0 if the attribute was successfully set; -1 otherwise
	*
	* @see element_meter()
	* @see ks_list_meter_int8_t()
	* @see ks_list_meter_int16_t()
	* @see ks_list_meter_int32_t()
	* @see ks_list_meter_int64_t()
	* @see ks_list_meter_uint8_t()
	* @see ks_list_meter_uint16_t()
	* @see ks_list_meter_uint32_t()
	* @see ks_list_meter_uint64_t()
	* @see ks_list_meter_float()
	* @see ks_list_meter_double()
	* @see ks_list_meter_string()
	*/
	int ks_list_attributes_copy(ks_list_t *restrict l, element_meter metric_fun, int copy_data);

	/**
	* set the element hash computing function for the list elements.
	*
	* [ advanced preference ]
	*
	* An hash can be requested depicting the list status at a given time. An hash
	* only depends on the elements and their order. By default, the hash of an
	* element is only computed on its reference. With this function, the user can
	* set a custom function computing the hash of an element. If such function is
	* provided, the list_hash() function automatically computes the list hash using
	* the custom function instead of simply referring to element references.
	*
	* @param l             list to operate
	* @param hash_computer_fun pointer to the actual hash computing function
	* @return              0 if the attribute was successfully set; -1 otherwise
	*
	* @see element_hash_computer()
	*/
	int ks_list_attributes_hash_computer(ks_list_t *restrict l, element_hash_computer hash_computer_fun);

	/**
	* set the element serializer function for the list elements.
	*
	* [ advanced preference ]
	*
	* Serialize functions are used for dumping the list to some persistent
	* storage.  The serializer function is called for each element; it is passed
	* a reference to the element and a reference to a ks_size_t object. It will
	* provide (and return) the buffer with the serialization of the element and
	* fill the ks_size_t object with the length of this serialization data.
	*
	* @param   l   list to operate
	* @param   serializer_fun  pointer to the actual serializer function
	* @return      0 if the attribute was successfully set; -1 otherwise
	*
	* @see     element_serializer()
	* @see     ks_list_dump_filedescriptor()
	* @see     ks_list_restore_filedescriptor()
	*/
	int ks_list_attributes_serializer(ks_list_t *restrict l, element_serializer serializer_fun);

	/**
	* set the element unserializer function for the list elements.
	*
	* [ advanced preference ]
	*
	* Unserialize functions are used for restoring the list from some persistent
	* storage. The unserializer function is called for each element segment read
	* from the storage; it is passed the segment and a reference to an integer.
	* It shall allocate and return a buffer compiled with the resumed memory
	* representation of the element, and set the integer value to the length of
	* this buffer.
	*
	* @param   l       list to operate
	* @param   unserializer_fun    pointer to the actual unserializer function
	* @return      0 if the attribute was successfully set; -1 otherwise
	*
	* @see     element_unserializer()
	* @see     ks_list_dump_filedescriptor()
	* @see     ks_list_restore_filedescriptor()
	*/
	int ks_list_attributes_unserializer(ks_list_t *restrict l, element_unserializer unserializer_fun);

	/**
	* append data at the end of the list.
	*
	* This function is useful for adding elements with a FIFO/queue policy.
	*
	* @param l     list to operate
	* @param data  pointer to user data to append
	*
	* @return      1 for success. < 0 for failure
	*/
	KS_DECLARE(int) ks_list_append(ks_list_t *restrict l, const void *data);

	/**
	* insert data in the head of the list.
	*
	* This function is useful for adding elements with a LIFO/Stack policy.
	*
	* @param l     list to operate
	* @param data  pointer to user data to append
	*
	* @return      1 for success. < 0 for failure
	*/
	KS_DECLARE(int) ks_list_prepend(ks_list_t *restrict l, const void * data);

	/**
	* extract the element in the top of the list.
	*
	* This function is for using a list with a FIFO/queue policy.
	*
	* @param l     list to operate
	* @return      reference to user datum, or NULL on errors
	*/
	KS_DECLARE(void *) ks_list_fetch(ks_list_t *restrict l);

	/**
	* retrieve an element at a given position.
	*
	* @param l     list to operate
	* @param pos   [0,size-1] position index of the element wanted
	* @return      reference to user datum, or NULL on errors
	*/
	KS_DECLARE(void *) ks_list_get_at(const ks_list_t *restrict l, unsigned int pos);

	/**
	* return the maximum element of the list.
	*
	* @warning Requires a comparator function to be set for the list.
	*
	* Returns the maximum element with respect to the comparator function output.
	*
	* @see ks_list_attributes_comparator()
	*
	* @param l     list to operate
	* @return      the reference to the element, or NULL
	*/
	KS_DECLARE(void *) ks_list_get_max(const ks_list_t *restrict l);

	/**
	* return the minimum element of the list.
	*
	* @warning Requires a comparator function to be set for the list.
	*
	* Returns the minimum element with respect to the comparator function output.
	*
	* @see ks_list_attributes_comparator()
	*
	* @param l     list to operate
	* @return      the reference to the element, or NULL
	*/
	KS_DECLARE(void *) ks_list_get_min(const ks_list_t *restrict l);

	/**
	* retrieve and remove from list an element at a given position.
	*
	* @param l     list to operate
	* @param pos   [0,size-1] position index of the element wanted
	* @return      reference to user datum, or NULL on errors
	*/
	KS_DECLARE(void *) ks_list_extract_at(ks_list_t *restrict l, unsigned int pos);

	/**
	* insert an element at a given position.
	*
	* @param l     list to operate
	* @param data  reference to data to be inserted
	* @param pos   [0,size-1] position index to insert the element at
	* @return      positive value on success. Negative on failure
	*/
	KS_DECLARE(int) ks_list_insert_at(ks_list_t *restrict l, const void *data, unsigned int pos);

	/**
	* expunge the first found given element from the list.
	*
	* Inspects the given list looking for the given element; if the element
	* is found, it is removed. Only the first occurence is removed.
	* If a comparator function was not set, elements are compared by reference.
	* Otherwise, the comparator is used to match the element.
	*
	* @param l     list to operate
	* @param data  reference of the element to search for
	* @return      0 on success. Negative value on failure
	*
	* @see ks_list_attributes_comparator()
	* @see ks_list_delete_at()
	*/
	KS_DECLARE(int) ks_list_delete(ks_list_t *restrict l, const void *data);

	/**
	* expunge an element at a given position from the list.
	*
	* @param l     list to operate
	* @param pos   [0,size-1] position index of the element to be deleted
	* @return      0 on success. Negative value on failure
	*/
	KS_DECLARE(int) ks_list_delete_at(ks_list_t *restrict l, unsigned int pos);

	/**
	* expunge an element at the current iterator position from the list.
	*
	* @param l     list to operate
	* @return      0 on success. Negative value on failure
	*/
	KS_DECLARE(int) ks_list_delete_iterator(ks_list_t *restrict l);

	/**
	* expunge an array of elements from the list, given their position range.
	*
	* @param l     list to operate
	* @param posstart  [0,size-1] position index of the first element to be deleted
	* @param posend    [posstart,size-1] position of the last element to be deleted
	* @return      the number of elements successfully removed on success, <0 on error
	*/
	KS_DECLARE(int) ks_list_delete_range(ks_list_t *restrict l, unsigned int posstart, unsigned int posend);

	/**
	* clear all the elements off of the list.
	*
	* The element datums will not be freed.
	*
	* @see ks_list_delete_range()
	* @see ks_list_size()
	*
	* @param l     list to operate
	* @return      the number of elements removed on success, <0 on error
	*/
	KS_DECLARE(int) ks_list_clear(ks_list_t *restrict l);

	/**
	* inspect the number of elements in the list.
	*
	* @param l     list to operate
	* @return      number of elements currently held by the list
	*/
	KS_DECLARE(unsigned int) ks_list_size(const ks_list_t *restrict l);

	/**
	* inspect whether the list is empty.
	*
	* @param l     list to operate
	* @return      0 iff the list is not empty
	*
	* @see ks_list_size()
	*/
	KS_DECLARE(int) ks_list_empty(const ks_list_t *restrict l);

	/**
	* find the position of an element in a list.
	*
	* @warning Requires a comparator function to be set for the list.
	*
	* Inspects the given list looking for the given element; if the element
	* is found, its position into the list is returned.
	* Elements are inspected comparing references if a comparator has not been
	* set. Otherwise, the comparator is used to find the element.
	*
	* @param l     list to operate
	* @param data  reference of the element to search for
	* @return      position of element in the list, or <0 if not found
	*
	* @see ks_list_attributes_comparator()
	* @see ks_list_get_at()
	*/
	KS_DECLARE(int) ks_list_locate(const ks_list_t *restrict l, const void *data, ks_bool_t prelocked);

	/**
	* returns an element given an indicator.
	*
	* @warning Requires a seeker function to be set for the list.
	*
	* Inspect the given list looking with the seeker if an element matches
	* an indicator. If such element is found, the reference to the element
	* is returned.
	*
	* @param l     list to operate
	* @param indicator indicator data to pass to the seeker along with elements
	* @return      reference to the element accepted by the seeker, or NULL if none found
	*/
	KS_DECLARE(void *) ks_list_seek(ks_list_t *restrict l, const void *indicator);

	/**
	* inspect whether some data is member of the list.
	*
	* @warning Requires a comparator function to be set for the list.
	*
	* By default, a per-reference comparison is accomplished. That is,
	* the data is in list if any element of the list points to the same
	* location of data.
	* A "semantic" comparison is accomplished, otherwise, if a comparator
	* function has been set previously, with ks_list_attributes_comparator();
	* in which case, the given data reference is believed to be in list iff
	* comparator_fun(elementdata, userdata) == 0 for any element in the list.
	*
	* @param l     list to operate
	* @param data  reference to the data to search
	* @return      0 iff the list does not contain data as an element
	*
	* @see ks_list_attributes_comparator()
	*/
	KS_DECLARE(int) ks_list_contains(const ks_list_t *restrict l, const void *data);

	/**
	* concatenate two lists
	*
	* Concatenates one list with another, and stores the result into a
	* user-provided list object, which must be different from both the
	* lists to concatenate. Attributes from the original lists are not
	* cloned.
	* The destination list referred is threated as virgin room: if it
	* is an existing list containing elements, memory leaks will happen.
	* It is OK to specify the same list twice as source, for "doubling"
	* it in the destination.
	*
	* @param l1    base list
	* @param l2    list to append to the base
	* @param dest  reference to the destination list
	* @return      0 for success, -1 for errors
	*/
	KS_DECLARE(int) ks_list_concat(const ks_list_t *l1, const ks_list_t *l2, ks_list_t *restrict dest);

	/**
	* sort list elements.
	*
	* @warning Requires a comparator function to be set for the list.
	*
	* Sorts the list in ascending or descending order as specified by the versus
	* flag. The algorithm chooses autonomously what algorithm is best suited for
	* sorting the list wrt its current status.
	*
	* @param l     list to operate
	* @param versus positive: order small to big; negative: order big to small
	* @return      0 iff sorting was successful
	*
	* @see ks_list_attributes_comparator()
	*/
	KS_DECLARE(int) ks_list_sort(ks_list_t *restrict l, int versus);

	/**
	* start an iteration session.
	*
	* This function prepares the list to be iterated.
	*
	* @param l     list to operate
	* @return 		0 if the list cannot be currently iterated. >0 otherwise
	*
	* @see ks_list_iterator_stop()
	*/
	KS_DECLARE(int) ks_list_iterator_start(ks_list_t *restrict l);

	/**
	* return the next element in the iteration session.
	*
	* @param l     list to operate
	* @return		element datum, or NULL on errors
	*/
	KS_DECLARE(void *) ks_list_iterator_next(ks_list_t *restrict l);

	/**
	* inspect whether more elements are available in the iteration session.
	*
	* @param l     list to operate
	* @return      0 iff no more elements are available.
	*/
	KS_DECLARE(int) ks_list_iterator_hasnext(const ks_list_t *restrict l);

	/**
	* end an iteration session.
	*
	* @param l     list to operate
	* @return      0 iff the iteration session cannot be stopped
	*/
	KS_DECLARE(int) ks_list_iterator_stop(ks_list_t *restrict l);

	/**
	* return the hash of the current status of the list.
	*
	* @param l     list to operate
	* @param hash  where the resulting hash is put
	*
	* @return      0 for success; <0 for failure
	*/
	KS_DECLARE(int) ks_list_hash(const ks_list_t *restrict l, ks_list_hash_t *restrict hash);

#ifndef SIMCLIST_NO_DUMPRESTORE
	/**
	* get meta informations on a list dump on filedescriptor.
	*
	* [ advanced function ]
	*
	* Extracts the meta information from a SimCList dump located in a file
	* descriptor. The file descriptor must be open and positioned at the
	* beginning of the SimCList dump block.
	*
	* @param fd        file descriptor to get metadata from
	* @param info      reference to a dump metainformation structure to fill
	* @return          0 for success; <0 for failure
	*
	* @see ks_list_dump_filedescriptor()
	*/
	int ks_list_dump_getinfo_filedescriptor(int fd, ks_list_dump_info_t *restrict info);

	/**
	* get meta informations on a list dump on file.
	*
	* [ advanced function ]
	*
	* Extracts the meta information from a SimCList dump located in a file.
	*
	* @param filename  filename of the file to fetch from
	* @param info      reference to a dump metainformation structure to fill
	* @return          0 for success; <0 for failure
	*
	* @see ks_list_dump_filedescriptor()
	*/
	int ks_list_dump_getinfo_file(const char *restrict filename, ks_list_dump_info_t *restrict info);

	/**
	* dump the list into an open, writable file descriptor.
	*
	* This function "dumps" the list to a persistent storage so it can be
	* preserved across process terminations.
	* When called, the file descriptor must be open for writing and positioned
	* where the serialized data must begin. It writes its serialization of the
	* list in a form which is portable across different architectures. Dump can
	* be safely performed on stream-only (non seekable) descriptors. The file
	* descriptor is not closed at the end of the operations.
	*
	* To use dump functions, either of these conditions must be satisfied:
	*      -# a metric function has been specified with ks_list_attributes_copy()
	*      -# a serializer function has been specified with ks_list_attributes_serializer()
	*
	* If a metric function has been specified, each element of the list is dumped
	* as-is from memory, copying it from its pointer for its length down to the
	* file descriptor. This might have impacts on portability of the dump to
	* different architectures.
	*
	* If a serializer function has been specified, its result for each element is
	* dumped to the file descriptor.
	*
	*
	* @param l     list to operate
	* @param fd    file descriptor to write to
	* @param len   location to store the resulting length of the dump (bytes), or NULL
	*
	* @return      0 if successful; -1 otherwise
	*
	* @see element_serializer()
	* @see ks_list_attributes_copy()
	* @see ks_list_attributes_serializer()
	*/
	int ks_list_dump_filedescriptor(const ks_list_t *restrict l, int fd, ks_size_t *restrict len);

	/**
	* dump the list to a file name.
	*
	* This function creates a filename and dumps the current content of the list
	* to it. If the file exists it is overwritten. The number of bytes written to
	* the file can be returned in a specified argument.
	*
	* @param l     list to operate
	* @param filename    filename to write to
	* @param len   location to store the resulting length of the dump (bytes), or NULL
	*
	* @return      0 if successful; -1 otherwise
	*
	* @see ks_list_attributes_copy()
	* @see element_serializer()
	* @see ks_list_attributes_serializer()
	* @see ks_list_dump_filedescriptor()
	* @see ks_list_restore_file()
	*
	* This function stores a representation of the list
	*/
	int ks_list_dump_file(const ks_list_t *restrict l, const char *restrict filename, ks_size_t *restrict len);

	/**
	* restore the list from an open, readable file descriptor to memory.
	*
	* This function is the "inverse" of ks_list_dump_filedescriptor(). It restores
	* the list content from a (open, read-ready) file descriptor to memory. An
	* unserializer might be needed to restore elements from the persistent
	* representation back into memory-consistent format. List attributes can not
	* be restored and must be set manually.
	*
	* @see ks_list_dump_filedescriptor()
	* @see ks_list_attributes_serializer()
	* @see ks_list_attributes_unserializer()
	*
	* @param l     list to restore to
	* @param fd    file descriptor to read from.
	* @param len   location to store the length of the dump read (bytes), or NULL
	* @return      0 if successful; -1 otherwise
	*/
	int ks_list_restore_filedescriptor(ks_list_t *restrict l, int fd, ks_size_t *restrict len);

	/**
	* restore the list from a file name.
	*
	* This function restores the content of a list from a file into memory. It is
	* the inverse of ks_list_dump_file().
	*
	* @see element_unserializer()
	* @see ks_list_attributes_unserializer()
	* @see ks_list_dump_file()
	* @see ks_list_restore_filedescriptor()
	*
	* @param l         list to restore to
	* @param filename  filename to read data from
	* @param len       location to store the length of the dump read (bytes), or NULL
	* @return          0 if successful; -1 otherwise
	*/
	int ks_list_restore_file(ks_list_t *restrict l, const char *restrict filename, ks_size_t *len);
#endif

	/* ready-made comparators, meters and hash computers */
	/* comparator functions */
	/**
	* ready-made comparator for int8_t elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_int8_t(const void *a, const void *b);

	/**
	* ready-made comparator for int16_t elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_int16_t(const void *a, const void *b);

	/**
	* ready-made comparator for int32_t elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_int32_t(const void *a, const void *b);

	/**
	* ready-made comparator for int64_t elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_int64_t(const void *a, const void *b);

	/**
	* ready-made comparator for uint8_t elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_uint8_t(const void *a, const void *b);

	/**
	* ready-made comparator for uint16_t elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_uint16_t(const void *a, const void *b);

	/**
	* ready-made comparator for uint32_t elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_uint32_t(const void *a, const void *b);

	/**
	* ready-made comparator for uint64_t elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_uint64_t(const void *a, const void *b);

	/**
	* ready-made comparator for float elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_float(const void *a, const void *b);

	/**
	* ready-made comparator for double elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_double(const void *a, const void *b);

	/**
	* ready-made comparator for string elements.
	* @see ks_list_attributes_comparator()
	*/
	int ks_list_comparator_string(const void *a, const void *b);

	/*          metric functions        */
	/**
	* ready-made metric function for int8_t elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_int8_t(const void *el);

	/**
	* ready-made metric function for int16_t elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_int16_t(const void *el);

	/**
	* ready-made metric function for int32_t elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_int32_t(const void *el);

	/**
	* ready-made metric function for int64_t elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_int64_t(const void *el);

	/**
	* ready-made metric function for uint8_t elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_uint8_t(const void *el);

	/**
	* ready-made metric function for uint16_t elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_uint16_t(const void *el);

	/**
	* ready-made metric function for uint32_t elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_uint32_t(const void *el);

	/**
	* ready-made metric function for uint64_t elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_uint64_t(const void *el);

	/**
	* ready-made metric function for float elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_float(const void *el);

	/**
	* ready-made metric function for double elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_double(const void *el);

	/**
	* ready-made metric function for string elements.
	* @see ks_list_attributes_copy()
	*/
	ks_size_t ks_list_meter_string(const void *el);

	/*          hash functions          */
	/**
	* ready-made hash function for int8_t elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_int8_t(const void *el);

	/**
	* ready-made hash function for int16_t elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_int16_t(const void *el);

	/**
	* ready-made hash function for int32_t elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_int32_t(const void *el);

	/**
	* ready-made hash function for int64_t elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_int64_t(const void *el);

	/**
	* ready-made hash function for uint8_t elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_uint8_t(const void *el);

	/**
	* ready-made hash function for uint16_t elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_uint16_t(const void *el);

	/**
	* ready-made hash function for uint32_t elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_uint32_t(const void *el);

	/**
	* ready-made hash function for uint64_t elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_uint64_t(const void *el);

	/**
	* ready-made hash function for float elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_float(const void *el);

	/**
	* ready-made hash function for double elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_double(const void *el);

	/**
	* ready-made hash function for string elements.
	* @see ks_list_attributes_hash_computer()
	*/
	ks_list_hash_t ks_list_hashcomputer_string(const void *el);

#ifdef __cplusplus
}
#endif

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
