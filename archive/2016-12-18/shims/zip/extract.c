//LibArchive support functions
//Taken almost verbatim from the LibArchive tutorial
#include "../../includes.h"

int copy_data(struct archive *ar, struct archive *aw)
{
	int r;
	const void *buff;
	size_t size;
	la_int64_t offset;

	for (;;) {
		r = archive_read_data_block(ar, &buff, &size, &offset);
		if (r == ARCHIVE_EOF){
			return (ARCHIVE_OK);
		}
		if (r < ARCHIVE_OK){
			return (r);
		}
		r = archive_write_data_block(aw, buff, size, offset);
		if (r < ARCHIVE_OK) {
			AlertMsg(archive_error_string(aw), "Archive Error");
			return (r);
		}
	}
}

BOOL extract(const char *filename, const char *outdir)
{
	struct archive *a;
	struct archive *ext;
	struct archive_entry *entry;
	int flags;
	int r;

	/* Select which attributes we want to restore. */
	flags = ARCHIVE_EXTRACT_TIME;
	flags |= ARCHIVE_EXTRACT_PERM;
	flags |= ARCHIVE_EXTRACT_ACL;
	flags |= ARCHIVE_EXTRACT_FFLAGS;

	//Create archive readers/writers
	a = archive_read_new();
	archive_read_support_format_zip(a);
	archive_read_support_compression_all(a);
	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);
	archive_write_disk_set_standard_lookup(ext);

	//Validate archive
	if ((r = archive_read_open_filename(a, filename, 10240))){
		AlertMsg(archive_error_string(a), "Archive Error");
		return FALSE;
	}

	for (;;){
		//Read file
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF){
			break;
		}
		if (r != ARCHIVE_OK){
			AlertMsg(archive_error_string(a), "Archive Error");
			return FALSE;
		}

		//Set output path
		{
			const char *path = archive_entry_pathname(entry);
			char *newPath = NULL;
			asprintf(&newPath, "%s/%s", outdir, path);
			archive_entry_set_pathname(entry, newPath);
			safe_free(newPath);
		}

		//Write file
		r = archive_write_header(ext, entry);
		
		if (r < ARCHIVE_OK){
			AlertMsg(archive_error_string(ext), "Archive Error");
			return FALSE;
		}
		else if (archive_entry_size(entry) > 0) {
			r = copy_data(a, ext);
			if (r != ARCHIVE_OK){
				AlertMsg(archive_error_string(ext), "Archive Error");
				return FALSE;
			}
		}
		
		r = archive_write_finish_entry(ext);
		
		if (r != ARCHIVE_OK){
			AlertMsg(archive_error_string(ext), "Archive Error");
			return FALSE;
		}
	}

	archive_read_close(a);
	archive_read_free(a);
	archive_write_close(ext);
	archive_write_free(ext);
	return TRUE;
}