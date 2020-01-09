void main();
void trace_output(struct ssd_info* );
void statistic_output(struct ssd_info *);
void free_all_node(struct ssd_info *);

struct ssd_info *warm_flash(struct ssd_info *ssd);
struct ssd_info *make_aged(struct ssd_info *);
struct ssd_info *pre_process_write(struct ssd_info *ssd);
struct ssd_info *process(struct ssd_info *);
struct ssd_info *simulate(struct ssd_info *);
void tracefile_sim(struct ssd_info *ssd);
void delete_update(struct ssd_info *ssd, struct sub_request *sub);

void file_assert(int error, char *s);
void alloc_assert(void *p, char *s);
void trace_assert(_int64 time_t, int device, unsigned int lsn, int size, int ope);


struct ssd_info *warm_flash(struct ssd_info *ssd);
void reset(struct ssd_info *ssd);
void Calculate_Energy(struct ssd_info *ssd);
struct ssd_info* sequece_write_warm_flash(struct ssd_info* ssd);