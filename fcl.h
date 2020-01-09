struct ssd_info *dynamic_advanced_process(struct ssd_info *ssd, unsigned int channel, unsigned int chip);
//struct ssd_info *delete_from_ssd(struct ssd_info *ssd, unsigned int channel, struct sub_request * sub_req);
struct ssd_info *delete_from_channel(struct ssd_info *ssd, unsigned int channel, struct sub_request * sub_req);
struct ssd_info* compute_serve_time(struct ssd_info* ssd, unsigned int channel, unsigned int chip, struct sub_request** subs, unsigned int subs_counts);
unsigned int find_read_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request **subs, unsigned int state, unsigned int command);

Status services_2_r_complete(struct ssd_info * ssd);
Status services_2_write(struct ssd_info * ssd, unsigned int channel);
Status service_2_read(struct ssd_info *ssd, unsigned int channel);
Status migration_horizon(struct ssd_info *ssd, unsigned int victim);
Status IS_superpage_valid(struct ssd_info *ssd, unsigned int sb_no, unsigned int page);

struct sub_request* get_first_plane_write_request(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane);
struct sub_request* get_first_die_write_request(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die);
struct sub_request* get_first_die_read_request(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die);
struct sub_request* get_first_plane_read_request(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane);

Status Add_mapping_entry(struct ssd_info* ssd, struct sub_request* sub);
Status Multi_Plane_Write(struct ssd_info* ssd, struct sub_request* sub0, struct sub_request* sub1);
Status Write(struct ssd_info* ssd, struct sub_request* sub);
int IS_Multi_Plane(struct ssd_info* ssd, struct sub_request* sub0, struct sub_request* sub1);

Status Update_read_req(struct ssd_info* ssd, struct sub_request* sub);
Status Multi_Plane_Read(struct ssd_info* ssd, struct sub_request* sub0, struct sub_request* sub1);
Status Read(struct ssd_info* ssd, struct sub_request* sub);
struct ssd_info* compute_read_serve_time(struct ssd_info* ssd, unsigned int channel, unsigned int chip, struct sub_request** subs, unsigned int subs_count);

int IS_Update_Done(struct ssd_info* ssd, struct sub_request* sub);