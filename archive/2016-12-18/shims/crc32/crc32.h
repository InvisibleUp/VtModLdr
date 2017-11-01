#ifdef __cplusplus
extern "C" {
#endif

static void make_crc_table(void);
const unsigned long * get_crc_table();
unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned long len);
unsigned long crc32File(const char *filename);

#ifdef __cplusplus
}
#endif