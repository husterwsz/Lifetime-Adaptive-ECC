struct ssd_info *pre_process_page(struct ssd_info *ssd);
struct local *find_location(struct ssd_info *ssd, unsigned int ppn);
struct ssd_info *get_ppn(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, struct sub_request *sub);
unsigned int get_ppn_for_pre_process(struct ssd_info *ssd, unsigned int lpn);
unsigned int find_ppn(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int page);
int  find_active_block(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane);
struct allocation_info* pre_process_allocation(struct ssd_info *ssd, unsigned int lpn);

Status find_active_superblock(struct ssd_info *ssd, unsigned int type);

Status migration(struct ssd_info *ssd, unsigned int victim);
Status SuperBlock_GC(struct ssd_info *ssd);
int find_victim_superblock(struct ssd_info *ssd);
int Get_SB_PE(struct ssd_info *ssd, unsigned int sb_no);
int Get_SB_Invalid(struct ssd_info *ssd, unsigned int sb_no);
Status Debug_Free_Block(struct ssd_info* ssd);
