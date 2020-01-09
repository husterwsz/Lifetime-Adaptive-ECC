#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "avlTree.h"

#define SECTOR 512
#define BUFSIZE 200
#define INDEX 10
#define PAGE_INDEX 1 //tlc mode .LSB/CSB/MSB
//#define PLANE_NUMBER 8 //for_plane_buffer
#define DIE_NUMBER 4
#define SAMPLE_SPACE 10

#define ALLOCATION_MOUNT 1
#define ALLOCATION_BUFFER 2

#define DYNAMIC_ALLOCATION 0
#define STATIC_ALLOCATION 1
#define HYBRID_ALLOCATION 2
#define SUPERBLOCK_ALLOCATION 3

#define PLANE_STATIC_ALLOCATION 0
#define SUPERPAGE_STATIC_ALLOCATION 1
#define CHANNEL_PLANE_STATIC_ALLOCATION 2
#define CHANNEL_SUPERPAGE_STATIC_ALLOCATION 3

#define CHANNEL_DYNAMIC_ALLOCATION 0
#define PLANE_DYNAMIC_ALLOCATION 1
#define STRIPE_DYNAMIC_ALLOCATION 2			//�����滻��˳����ѯ���䵽ÿ��die_buffer����
#define OSPA_DYNAMIC_ALLOCATION 3			//���վ���Զ�ķ��������䵽����Զ��die_buffer����
#define POLL_DISTRANCE_ALLOCATION 4			//�ۺ���ѯ�;���İ취�����߼�ˣ�����̫����������ǰ��ѯ����һ��

#define SLC_MODE 0
#define TLC_MODE 1

#define MUTLI_PLANE 0
#define NORMAL    1
#define HALF_PAGE 2
#define ONE_SHOT 3
#define ONE_SHOT_MUTLI_PLANE 4
#define ONE_SHOT_READ 5
#define ONE_SHOT_READ_MUTLI_PLANE 6

//advanced command
#define AD_MUTLIPLANE  1  //mutli plane 
#define AD_HALFPAGE_READ 2  //half page read ,only used in single plane read operation
#define AD_ONESHOT_PROGRAM 4  //one shot program ,used in mutli plane and single plane program operation
#define AD_ONESHOT_READ 8     //one shot read ,used in mutli plane and single plane program operation
#define AD_ERASE_SUSPEND_RESUME 16  //erase_suspend/resume,used in erase operation

#define READ 1
#define WRITE 0

#define FCFS 1

#define SIG_NORMAL 11
#define SIG_ERASE_WAIT 12
#define SIG_ERASE_SUSPEND 13
#define SIG_ERASE_RESUME 14

#define NORMAL_TYPE 0
#define SUSPEND_TYPE 1

#define WRITE_MORE 1      
#define READ_MORE 2

#define SSD_MOUNT 1
#define CHANNEL_MOUNT 2
/*********************************all states of each objects************************************************
*Defines the channel idle, command address transmission, data transmission, transmission, and other states
*And chip free, write busy, read busy, command address transfer, data transfer, erase busy, copyback busy, 
*other status , And read and write requests (sub) wait, read command address transfer, read, read data transfer, 
*write command address transfer, write data transfer, write transfer, completion and other status
************************************************************************************************************/

#define CHANNEL_IDLE 000
#define CHANNEL_C_A_TRANSFER 3
#define CHANNEL_GC 4           
#define CHANNEL_DATA_TRANSFER 7
#define CHANNEL_TRANSFER 8
#define CHANNEL_UNKNOWN 9

#define CHIP_IDLE 100
#define CHIP_WRITE_BUSY 101
#define CHIP_READ_BUSY 102
#define CHIP_C_A_TRANSFER 103
#define CHIP_DATA_TRANSFER 107
#define CHIP_WAIT 108
#define CHIP_ERASE_BUSY 109

#define SR_WAIT 200                 
#define SR_R_C_A_TRANSFER 201
#define SR_R_READ 202
#define SR_R_DATA_TRANSFER 203
#define SR_W_C_A_TRANSFER 204
#define SR_W_DATA_TRANSFER 205
#define SR_W_TRANSFER 206
#define SR_COMPLETE 299

#define REQUEST_IN 300         //Next request arrival time
#define OUTPUT 301             //The next time the data is output

#define GC_WAIT 400
#define GC_ERASE_C_A 401
#define GC_COPY_BACK 402
#define GC_COMPLETE 403
#define GC_INTERRUPT 0
#define GC_UNINTERRUPT 1

#define CHANNEL(lsn) (lsn&0x0000)>>16      
#define chip(lsn) (lsn&0x0000)>>16 
#define die(lsn) (lsn&0x0000)>>16 
#define PLANE(lsn) (lsn&0x0000)>>16 
#define BLOKC(lsn) (lsn&0x0000)>>16 
#define PAGE(lsn) (lsn&0x0000)>>16 
#define SUBPAGE(lsn) (lsn&0x0000)>>16  

#define PG_SUB 0xffffffff			

//define READ OPERATION
#define REQ_READ 0
#define UPDATE_READ 1
#define GC_READ 2


#define SET_VALID(s,i) ((1<<i)|s)
#define GET_BIT(s,i)    ((s>>i)&1)

//define superblock infomation 
#define PLANE_LEVEL 1
#define DIE_LEVEL 2
#define CHIP_LEVEL 3
#define CHANNEL_LEVEL 4

#define NAX_SB_SIZE 128
#define MIN_SB_RATE 0.04
#define INVALID_PPN -1                                                

//calculate power
   //dram
#define DRAM_POWER_32MB_LOW 114.1 //unit is mw
#define DRAM_POWER_32MB_MODERATE 380.0 //unit is mw
#define DRAM_POWER_32MB_HIGH 689.5
  //flash
#define PROGRAM_POWER_PAGE 29.747 //unit is uj
#define READ_POWER_PAGE 1.632 
#define ERASE_POWER_BLOCK 232.908 

//mapping schemes 
#define PAGE_MAPPING 0
#define BLOCK_MAPPING 1
#define HYBRID_MAPPING 2
#define DFTL_MAPPING 3

//dram power schemes
#define LOW 0
#define MODERATE 1
#define HIGH 2

#define B 1
#define KB (1024*B)
#define MB (1024*KB)
#define GB (1024*MB)
#define USER_SIZE (16*GB)


#define ns 1
#define us (1000*ns)
#define ms (1000*us)
#define S (1000*ms)

#define uJ 1
#define mJ (1000*uJ)
#define J (1000*mJ)

#define SB_LEVEL DIE_LEVEL

/* 1:plane-level superblock
2:die-level superblock
3:chip-level superblock
4:channel-level superblock
*/

#define TRAN_BLK 1
#define DATA_BLK 0
#define DATA_COMMAND_BUFFER 0
#define TRANSACTION_COMMAND_BUFFER 1
#define CACHE_VALID 1
#define CACHE_INVALID 0
#define USER_DATA 0
#define MAPPING_DATA 1

#define FULLY_CHACHE 0
#define DFTL 1
#define LEVEL_FTL 2

/*
    16 GB SSD  = 4 M flash pages
*/

//#define CACHE_SIZE 1024 
#define MAX_CACHE_SIZE  (4*1024*1024)
#define MAX_LSN_NUM 64

#define MAX_LUN_PER_PAGE 4

/*************************************************************************
*Function result status code
*Status is the function type ,the value is the function result status code
**************************************************************************/
#define TRUE		1
#define FALSE		0
#define SUCCESS		1
#define FAILURE		0
#define ERROR		-1
#define INFEASIBLE	-2
#define OVERFLOW	-3
typedef int Status;     

struct ac_time_characteristics{
	int tPROG;     //program time
	int tDBSY;     //bummy busy time for two-plane program
	int tBERS;     //block erase time
	int tPROGO;    //one shot program time
	int tERSL;	   //the trans time of suspend / resume operation
	int tCLS;      //CLE setup time
	int tCLH;      //CLE hold time
	int tCS;       //CE setup time
	int tCH;       //CE hold time
	int tWP;       //WE pulse width
	int tALS;      //ALE setup time
	int tALH;      //ALE hold time
	int tDS;       //data setup time
	int tDH;       //data hold time
	int tWC;       //write cycle time
	int tWH;       //WE high hold time
	int tADL;      //address to data loading time
	int tR;        //data transfer from cell to register
	int tAR;       //ALE to RE delay
	int tCLR;      //CLE to RE delay
	int tRR;       //ready to RE low
	int tRP;       //RE pulse width
	int tWB;       //WE high to busy
	int tRC;       //read cycle time
	int tREA;      //RE access time
	int tCEA;      //CE access time
	int tRHZ;      //RE high to output hi-z
	int tCHZ;      //CE high to output hi-z
	int tRHOH;     //RE high to output hold
	int tRLOH;     //RE low to output hold
	int tCOH;      //CE high to output hold
	int tREH;      //RE high to output time
	int tIR;       //output hi-z to RE low
	int tRHW;      //RE high to WE low
	int tWHR;      //WE high to RE low
	int tRST;      //device resetting time
}ac_timing;


struct ssd_info{ 
	//Global variable
	int make_age_free_page;				 //The number of free pages after make_aged
	int buffer_full_flag;				 //buffer blocking flag:0--unblocking , 1-- blocking
	int trace_over_flag;				 //the end of trace flag:0-- not ending ,1--ending
	__int64 request_lz_count;			 //trace request count
	unsigned int update_sub_request;
	unsigned int page_count;
	int test_count;
	unsigned int die_token;
	unsigned int plane_count;
	int warm_flash_cmplt;
	
	//superblock info
	int sb_cnt;
	int free_sb_cnt;
	struct super_block_info *sb_pool;
	struct super_block_info *open_sb[2]; //0 is for user data ; 1 is for mapping data

	__int64 current_time;                //Record system time
	__int64 next_request_time;
	unsigned int real_time_subreq;       //Record the number of real-time write requests, used in the full dynamic allocation, channel priority situation

	int flag;
	unsigned int page;

	unsigned int token;                  //In the dynamic allocation, in order to prevent each assignment in the first channel need to maintain a token, each time from the token refers to the location of the distribution
	unsigned int gc_request;             //Recorded in the SSD, the current moment how many gc operation request

	__int64 write_avg;                   //Record the time to calculate the average response time for the write request
	__int64 read_avg;                    //Record the time to calculate the average response time for the read request

	unsigned int min_lsn;
	unsigned int max_lsn;


	//read type
	unsigned long read_count;             //Record the number of read
	unsigned long req_read_count;         //Record the number of request read.
	unsigned long update_read_count;      //Record the number of updates read
	unsigned long gc_read_count;		  //Record gc caused by the read operation

	//read hit
	unsigned long gc_read_hit_cnt;
	unsigned long update_read_hit_cnt;
	unsigned long req_read_hit_cnt;

	unsigned long half_page_read_count;   //Recond the number of half page read operation
	unsigned long one_shot_read_count;	  //Recond the number of one shot read operation
	unsigned long one_shot_mutli_plane_count;//Record the number of one shot mutli plane read operation
	unsigned long resume_count;
	unsigned long suspend_count;
	unsigned long suspend_read_count;

	unsigned long program_count;
	unsigned long pre_all_write;		 //Record preprocessing write operation
	unsigned long update_write_count;	 //Record the number of updates write
	unsigned long gc_write_count;		 //Record gc caused by the write operation

	unsigned long erase_count;
	unsigned long direct_erase_count;    //Record invalid blocks that are directly erased
	__int64 gc_count;

	//Advanced command read and write erase statistics
	unsigned long m_plane_read_count;
	unsigned long m_plane_prog_count;
	unsigned long mplane_erase_count;

	unsigned long ontshot_prog_count;
	unsigned long mutliplane_oneshot_prog_count;

	unsigned long write_flash_count;     //The actual write to the flash
	unsigned long waste_page_count;      //Record the page waste due to restrictions on advanced commands

	unsigned long write_request_count;    //Record the number of write operations
	unsigned long read_request_count;     //Record the number of read operations
	
	float ave_read_size;
	float ave_write_size;
	unsigned int request_queue_length;
	
	char parameterfilename[50];
	char tracefilename[50];
	char outputfilename[50];
	char statisticfilename[50];
	char statistic_time_filename[50];
	char statistic_size_filename[50];
	//char die_read_req_name[50];

	FILE * outputfile;
	FILE * tracefile;
	FILE * statisticfile;
	FILE * statisticfile_time;
	FILE * statisticfile_size;
	FILE * sb_info;
	FILE * die_read_req;
	FILE * read_req;
	FILE* smt;

    struct parameter_value *parameter;   //SSD parameter
	struct dram_info *dram;
	struct request *request_queue;       //dynamic request queue
	struct request *request_head;		 // the head of the request queue
	struct request *request_tail;	     // the tail of the request queue
	struct request *request_work;		 // the work point of the request queue
	struct sub_request *subs_w_head;     //When using the full dynamic allocation, the first hanging on the ssd, etc. into the process function is linked to the corresponding channel read request queue
	struct sub_request *subs_w_tail;
	struct channel_info *channel_head;   //Points to the first address of the channel structure array

	int warm_flash_flag;


	//dftl 
	unsigned int cache_hit;
	unsigned int read_cache_hit_num;
	unsigned int write_cache_hit_num;
	unsigned int write_evict;
	unsigned int read_evict;
	unsigned int evict;
	unsigned int cache_size;
	unsigned int data_read_cnt;
	unsigned int tran_read_cnt;
	unsigned int data_update_cnt;
	unsigned int tran_update_cnt;
	unsigned int tran_req_cnt;
	unsigned int data_req_cnt;
	unsigned int data_program_cnt;
	unsigned int tran_program_cnt;
};


struct channel_info{
	int chip;                            //Indicates how many particles are on the bus
	
	int current_state;                   //channel has serveral states, including idle, command/address transfer,data transfer
	int next_state;
	__int64 current_time;                //Record the current time of the channel
	__int64 next_state_predict_time;     //the predict time of next state, used to decide the sate at the moment

	struct sub_request *subs_r_head;     //The read request on the channel queue header, the first service in the queue header request
	struct sub_request *subs_r_tail;     //Channel on the read request queue tail, the new sub-request added to the tail
	struct sub_request *subs_w_head;     //The write request on the channel queue header, the first service in the queue header request
	struct sub_request *subs_w_tail;     //The write request queue on the channel, the new incoming request is added to the end of the queue
	struct gc_operation *gc_command;     //Record the need to generate gc position

	unsigned int gc_soft;
	unsigned int gc_hard;
	unsigned int gc_stop;
	
	unsigned int channel_busy_flag;
	unsigned long channel_read_count;	 //Record the number of read and write wipes within the channel
	unsigned long channel_program_count;
	unsigned long channel_erase_count;

	struct chip_info *chip_head;        
};


struct chip_info{
	unsigned int die_num;               //Indicates how many die is in a chip
	unsigned int plane_num_die;         //indicate how many planes in a die
	unsigned int block_num_plane;       //indicate how many blocks in a plane
	unsigned int page_num_block;        //indicate how many pages in a block
	unsigned int subpage_num_page;      //indicate how many subpage in a page
	unsigned int ers_limit;             //The number of times each block in the chip can be erased              

	int current_state;                  //chip has serveral states, including idle, command/address transfer,data transfer,unknown
	int next_state;            
	__int64 current_time;               //Record the current time of the chip
	__int64 next_state_predict_time;    //the predict time of next state, used to decide the sate at the moment
	
	int gc_signal;
	__int64 erase_begin_time;            
	__int64 erase_cmplt_time;
	__int64 erase_rest_time;
	struct suspend_location *suspend_location;

	unsigned long chip_read_count;      //Record the number of read/program/erase in the chip
	unsigned long chip_program_count;
	unsigned long chip_erase_count;

    struct ac_time_characteristics ac_timing;  
	struct die_info *die_head;
};


struct die_info{
	unsigned long die_read_count;		//Record the number of read/program/erase in the die
	unsigned long die_program_count;
	unsigned long die_erase_count;

	unsigned int read_cnt;
	struct plane_info *plane_head;
	
};


struct plane_info{
	int add_reg_ppn;                    //Read, write address to the variable, the variable represents the address register. When the die is changed from busy to idle, clear the address
	unsigned int free_page;             //the number of free page in plane
	unsigned int ers_invalid;           //Record the number of blocks in the plane that are erased
	unsigned int active_block;          //The physical block number of the active block
	int can_erase_block;                //Record in a plane prepared in the gc operation was erased block, -1 that has not found a suitable block

	unsigned long plane_read_count;		//Record the number of read/program/erase in the plane
	unsigned long plane_program_count;
	unsigned long plane_erase_count;
	unsigned long pre_plane_write_count;
	
	unsigned int test_gc_count;
	unsigned int test_pro_count;
	unsigned int test_pre_count;

	struct direct_erase *erase_node;    //Used to record can be directly deleted block number, access to the new ppn, whenever the invalid_page_num == 64, it will be added to the pointer, for the GC operation directly delete
	struct blk_info *blk_head;
};


struct blk_info{
	unsigned int erase_count;          //The number of erasures for the block, which is recorded in ram for the GC
	unsigned int page_read_count;	   //Record the number of read pages of the block
	unsigned int page_write_count;	   //Record the number of write pages
	unsigned int pre_write_count;	   //Record the number of times the prepress was written

	unsigned int free_page_num;        //Record the number of pages in the block
	unsigned int invalid_page_num;     //Record the number of invaild pages in the block
	int last_write_page;               //Records the number of pages executed by the last write operation, and -1 indicates that no page has been written
	struct page_info *page_head; 

	unsigned int block_type;
};


struct page_info{                      //lpn records the physical page stored in the logical page, when the logical page is valid, valid_state>0, free_state>0
	double valid_state;                   //indicate the page is valid or invalid
	int free_state;                    //each bit indicates the subpage is free or occupted. 1 indicates that the bit is free and 0 indicates that the bit is used
	int lpn;                 
	unsigned int written_count;        //Record the number of times the page was written
};

struct super_block_info{
	int ec; //min ec
	int blk_cnt; //superblock size
	struct local *pos;
	int next_wr_page; // next available page
	int pg_off; //used page offset in the superpage 
	int blk_type;
};

struct dram_info{
	unsigned int dram_capacity;     
	__int64 current_time;

	struct dram_parameter *dram_paramters;      
	struct map_info *map;   //mapping for user data
	struct map_info *tran_map;  //mapping for transaction data 

	struct buffer_info *data_buffer;  //data buffer 
	struct buffer_info* mapping_buffer; //mapping_buffer
	struct buffer_info *data_command_buffer;					 //data commond buffer
	struct buffer_info *mapping_command_buffer;               //translation commond buffer
};


/*****************************************************************************************************************************************
*Buff strategy:Blocking buff strategy
*1--first check the buffer is full, if dissatisfied, check whether the current request to put down the data, if so, put the current request, 
*if not, then block the buffer;
*
*2--If buffer is blocked, select the replacement of the two ends of the page. If the two full page, then issued together to lift the buffer 
*block; if a partial page 1 full page or 2 partial page, then issued a pre-read request, waiting for the completion of full page and then issued 
*And then release the buffer block.
********************************************************************************************************************************************/
typedef struct buffer_group{
	TREE_NODE node;                     //The structure of the tree node must be placed at the top of the user-defined structure
	struct buffer_group *LRU_link_next;	// next node in LRU list
	struct buffer_group *LRU_link_pre;	// previous node in LRU list

	unsigned int group;                 //the first data logic sector number of a group stored in buffer 
	unsigned int stored;                //indicate the sector is stored in buffer or not. 1 indicates the sector is stored and 0 indicate the sector isn't stored.EX.  00110011 indicates the first, second, fifth, sixth sector is stored in buffer.
	unsigned int dirty_clean;           //it is flag of the data has been modified, one bit indicates one subpage. EX. 0001 indicates the first subpage is dirty
	int flag;			                //indicates if this node is the last 20% of the LRU list	
	unsigned int page_type;				//buff page type:0--full_page  1--partial_page

	unsigned int lsns[MAX_LSN_NUM];
	int state[MAX_LSN_NUM];
	unsigned int lsn_count;
}buf_node;


struct dram_parameter{
	float active_current;
	float sleep_current;
	float voltage;
	int clock_time;
};


struct map_info{
	struct entry *map_entry;            // each entry indicate a mapping information
};


struct controller_info{
	unsigned int frequency;             //Indicates the operating frequency of the controller
	__int64 clock_time;                 //Indicates the time of a clock cycle
	float power;                        //Indicates the energy consumption per unit time of the controller
};


struct request{
	__int64 time;                      //Request to reach the time(us)
	unsigned int lsn;                  //The starting address of the request, the logical address
	unsigned int size;                 //The size of the request, the number of sectors
	unsigned int operation;            //The type of request, 1 for the read, 0 for the write
	unsigned int cmplt_flag;		   //Whether the request is executed, 0 means no execution, 1 means it has been executed

	unsigned int* need_distr_flag;
	unsigned int complete_lsn_count;   //record the count of lsn served by buffer

	int distri_flag;		           // indicate whether this request has been distributed already

	__int64 begin_time;
	__int64 response_time;
	__int64 request_read_num;

	struct sub_request *subs;          //Record all sub-requests belonging to the request
	struct request *next_node;        
};


struct sub_request{
	unsigned int lpn;                  //The logical page number of the sub request
	unsigned int ppn;                  //The physical page number of the request
	unsigned int operation;            //Indicates the type of the sub request, except that read 1 write 0, there are erase, two plane and other operations
	int size;

	unsigned int current_state;        //Indicates the status of the subquery
	__int64 current_time;
	unsigned int next_state;
	__int64 next_state_predict_time;
	 unsigned int state;              //The requested sector status bit

	__int64 begin_time;               //Sub request start time
	__int64 complete_time;            //Record the processing time of the sub-request, the time that the data is actually written or read out

	unsigned int req_type;   //mark sub request type

	unsigned int luns[MAX_LUN_PER_PAGE];
	unsigned int lun_count;
	unsigned int lun_state[MAX_LUN_PER_PAGE];

	struct local *location;           //In the static allocation and mixed allocation mode, it is known that lpn knows that the lpn is assigned to that channel, chip, die, plane, which is used to store the calculated address
	struct sub_request *next_subs;    //Points to the child request that belongs to the same request
	struct sub_request *next_node;    //Points to the next sub-request structure in the same channel
	struct sub_request *update;       //Update the write request, mount this pointer on

	struct request *total_request;

	unsigned int update_read_flag;    //Update the read flag
	unsigned int mutliplane_flag;
	unsigned int oneshot_flag;
	unsigned int oneshot_mutliplane_flag;

	//suspend
	unsigned int suspend_req_flag;
	struct sub_request *next_suspend_sub;

	//gc req flag
	int gc_flag;
	int read_flag;
};


struct parameter_value{
	unsigned int chip_num;          //the number of chip in ssd
	unsigned int dram_capacity;     //Record the DRAM capacity in SSD
	unsigned int cpu_sdram;         //sdram capacity in cpu

	unsigned int channel_number;    //Record the number of channels in the SSD, each channel is a separate bus
	unsigned int chip_channel[100]; //Set the number of channels in the SSD and the number of particles on each channel

	unsigned int die_chip;    
	unsigned int plane_die;
	unsigned int block_plane;
	unsigned int page_block;
	unsigned int subpage_page;

	unsigned int page_capacity;
	unsigned int subpage_capacity;
	unsigned int mapping_entry_size;


	unsigned int ers_limit;         //Record the number of erasable blocks per block
	int address_mapping;            //Record the type of mapping,1��page��2��block��3��fast
	int wear_leveling;              //WL algorithm 
	int gc;                         //Record gc strategy
	int clean_in_background;        //Whether the cleanup operation is done in the foreground
	int alloc_pool;                 //allocation pool ��С(plane��die��chip��channel),Ҳ����ӵ��active_block�ĵ�λ
	float overprovide;

	double operating_current;       //NAND FLASH operating current(uA)
	double supply_voltage;	
	double dram_active_current;     //cpu sdram work current   uA
	double dram_standby_current;    //cpu sdram work current   uA
	double dram_refresh_current;    //cpu sdram work current   uA
	double dram_voltage;            //cpu sdram work voltage  V

	int buffer_management;          //indicates that there are buffer management or not
	int scheduling_algorithm;       //Record which scheduling algorithm to use��1:FCFS
	float quick_radio;
	int related_mapping;

	unsigned int time_step;
	unsigned int small_large_write; //the threshould of large write, large write do not occupt buffer, which is written back to flash directly

	int striping;                   //Indicates whether or not striping is used, 0--unused, 1--used
	int interleaving;
	int pipelining;
	int threshold_fixed_adjust;
	int threshold_value;
	int active_write;               //Indicates whether an active write operation is performed,1,yes;0,no
	float gc_hard_threshold;        //Hard trigger gc threshold size
	float gc_soft_threshold;        //Soft trigger gc threshold size
	int allocation_scheme;          //Record the choice of allocation mode, 0 for dynamic allocation, 1 for static allocation
	int static_allocation;          //The record is the kind of static allocation
	int dynamic_allocation;			 //The priority of the dynamic allocation 0--channel>chip>die>plane,1--plane>channel>chip>die
	int advanced_commands;  
	int ad_priority;                //record the priority between two plane operation and interleave operation
	int greed_MPW_ad;               //0 don't use multi-plane write advanced commands greedily; 1 use multi-plane write advanced commands greedily
	int aged;                       //1 indicates that the SSD needs to be aged, 0 means that the SSD needs to be kept non-aged
	float aged_ratio; 
	int queue_length;               //Request the length of the queue
	int warm_flash;
	int update_reqeust_max;		    //request the length of sub request(partial page)
	int flash_mode;                 //0--slc mode,1--tlc mode

	struct ac_time_characteristics time_characteristics;
};

/********************************************************
*mapping information,The highest bit of state indicates 
*whether there is an additional mapping relationship
*********************************************************/
struct entry{                       
	unsigned int pn;                //Physical number, either a physical page number, a physical subpage number, or a physical block number
	int state;                      //The hexadecimal representation is 0000-FFFF, and each bit indicates whether the corresponding subpage is valid (page mapping). 
	unsigned int cache_valid;       // 0 means no cached; 1 means cacheed.
	unsigned int cache_age;         //is for cache replacement
	unsigned int update;            //is for judge wether the entry is dirty
};


struct local{          
	unsigned int channel;
	unsigned int chip;
	unsigned int die;
	unsigned int plane;
	unsigned int block;
	unsigned int page;
	unsigned int sub_page;
};


struct gc_info{
	__int64 begin_time;            //Record a plane when to start gc operation
	int copy_back_count;    
	int erase_count;
	__int64 process_time;          //Record time the plane spent on gc operation
	double energy_consumption;     //Record energy the plane takes on gc
};


struct direct_erase{
	unsigned int block;
	struct direct_erase *next_node;
};


/**************************************************************************************
 *When a GC operation is generated, the structure is hung on the corresponding channel 
 *and the GC operation command is issued when the channel is idle
***************************************************************************************/
struct gc_operation{  
	unsigned int channel;
	unsigned int chip;
	unsigned int die;
	unsigned int plane;
	unsigned int block;           //This parameter is used only in the interruptable gc function (gc_interrupt), used to record the near-identified target block number
	unsigned int page;            //This parameter is used only in the interruptable gc function (gc_interrupt), used to record the page number of the data migration that has been completed
	unsigned int state;           //Record the status of the current gc request
	unsigned int priority;        //Record the priority of the gc operation, 1 that can not be interrupted, 0 that can be interrupted (soft threshold generated by the gc request)
	unsigned int hard;
	unsigned int soft;

	struct gc_operation *next_node;
};

struct suspend_location{
	unsigned int channel;
	unsigned int chip;
	unsigned int die;
	unsigned int plane[2];
	unsigned int block[2];
};


struct allocation_info                       //��¼������Ϣ
{
	unsigned int channel;
	unsigned int chip;
	unsigned int die;
	unsigned int plane;
	unsigned int mount_flag;
	struct buffer_info * aim_command_buffer;
};

struct ssd_info *initiation(struct ssd_info *);
struct parameter_value *load_parameters(char parameter_file[30]);
struct page_info * initialize_page(struct page_info * p_page);
struct blk_info * initialize_block(struct blk_info * p_block,struct parameter_value *parameter);
struct plane_info * initialize_plane(struct plane_info * p_plane,struct parameter_value *parameter );
struct die_info * initialize_die(struct die_info * p_die,struct parameter_value *parameter,long long current_time );
struct chip_info * initialize_chip(struct chip_info * p_chip,struct parameter_value *parameter,long long current_time );
struct ssd_info * initialize_channels(struct ssd_info * ssd );
struct dram_info * initialize_dram(struct ssd_info * ssd);
void initialize_statistic(struct ssd_info * ssd);
void show_sb_info(struct ssd_info * ssd);
void intialize_sb(struct ssd_info * ssd);
int Get_Channel(struct ssd_info * ssd, int i);
int Get_Chip(struct ssd_info * ssd, int i);
int Get_Die(struct ssd_info * ssd, int i);
int Get_Plane(struct ssd_info * ssd, int i);
int Get_Read_Request_Cnt(struct ssd_info *ssd, unsigned int chan, unsigned int chip, unsigned int die);
void Show_Die_Read_Req(struct ssd_info *ssd);
Status Read_cnt_4_Debug(struct ssd_info *ssd);