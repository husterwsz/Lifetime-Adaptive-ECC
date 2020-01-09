int erase_operation(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block);
int move_page(struct ssd_info * ssd, struct local *location, unsigned int move_plane, unsigned int * transfer_size);
int write_page(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int active_block, unsigned int *ppn);

struct ssd_info *flash_page_state_modify(struct ssd_info *, struct sub_request *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
int  NAND_program(struct ssd_info *ssd, struct sub_request * req);
int  NAND_read(struct ssd_info *ssd, struct sub_request * req);
int NAND_multi_plane_program(struct ssd_info* ssd, struct sub_request* req0, struct sub_request* req1);

int IS_Update_Done(struct ssd_info* ssd, struct sub_request* sub);