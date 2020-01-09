#define _CRTDBG_MAP_ALLOC

#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "ssd.h"
#include "initialize.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"
#include "assert.h"
extern int secno_num_per_page, secno_num_sub_page;

/**********************************************************************************************************************************************
*Buff strategy:Blocking buff strategy
*1--first check the buffer is full, if dissatisfied, check whether the current request to put down the data, if so, put the current request,
*if not, then block the buffer;
*
*2--If buffer is blocked, select the replacement of the two ends of the page. If the two full page, then issued together to lift the buffer
*block; if a partial page 1 full page or 2 partial page, then issued a pre-read request, waiting for the completion of full page and then issued
*And then release the buffer block.
***********************************************************************************************************************************************/
struct ssd_info *buffer_management(struct ssd_info *ssd)
{
	struct request *new_request;

#ifdef DEBUG
	printf("enter buffer_management,  current time:%I64u\n", ssd->current_time);
#endif

	ssd->dram->current_time = ssd->current_time;
	new_request = ssd->request_work; 

	handle_write_buffer(ssd, new_request);
	if (new_request->subs == NULL)  //sub requests are cached in data buffer 
	{
		new_request->begin_time = ssd->current_time;
		new_request->response_time = ssd->current_time + 1000;
	}

	new_request->cmplt_flag = 1;
	ssd->buffer_full_flag = 0;
	return ssd;
}

struct ssd_info *handle_write_buffer(struct ssd_info *ssd, struct request *req)
{
	unsigned int full_page, lsn, lun, last_lun, first_lun,i;
	unsigned int mask;
	unsigned int state,offset1 = 0, offset2 = 0, flag = 0;                                                                                       

	lsn = req->lsn;
	last_lun = (req->lsn + req->size - 1) / secno_num_sub_page;
	first_lun = req->lsn/ secno_num_sub_page;
	lun = first_lun;

	while (lun <= last_lun)     
	{
		state = 0; 
		offset1 = 0;
		offset2 = secno_num_sub_page - 1;
/*
		if (lun == first_lun)
			offset1 = lsn - lun* secno_num_sub_page;
		if (lun == last_lun)
			offset2 = (lsn + req->size - 1) % ssd->parameter->subpage_page;
*/

		for (i = offset1; i <= offset2; i++)
			state = SET_VALID(state, i);

		if (req->operation == READ)                                                   
			ssd = check_w_buff(ssd, lun, state, NULL, req);
		else if (req->operation == WRITE)
			ssd = insert2buffer(ssd, lun, state, NULL, req);
		lun++;
	}
	return ssd;
}

struct ssd_info *handle_read_cache(struct ssd_info *ssd, struct request *req)           //处理读缓存，待添加
{
	return ssd;
}

struct ssd_info * check_w_buff(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	struct buffer_group *buffer_node, key;
	struct sub_request *sub_req = NULL;

	key.group = lpn;
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->data_buffer, (TREE_NODE *)&key);

	if (buffer_node == NULL)      
	{
		sub_req = NULL;
		sub_req_state = state;
		sub_req_size = size(state);
		sub_req_lpn = lpn;
		//sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, READ,DATA_COMMAND_BUFFER);  
		read_reqeust(ssd, sub_req_lpn, req, state, USER_DATA);

		ssd->dram->data_buffer->read_miss_hit++;         
	}
	else
	{
		if ((state&buffer_node->stored) == state)   
		{
			ssd->dram->data_buffer->read_hit++;
		}
		else    
		{
			sub_req = NULL;
			sub_req_state = (state | buffer_node->stored) ^ buffer_node->stored;      
			sub_req_size = size(sub_req_state);    
			sub_req_lpn = lpn;
			//sub_req = creat_sub_request(ssd, sub_req_lpn, sub_req_size, sub_req_state, req, READ,DATA_COMMAND_BUFFER);
			read_reqeust(ssd, sub_req_lpn, req, state, USER_DATA);

			ssd->dram->data_buffer->read_miss_hit++;
		}
	}
	return ssd;
}

/*******************************************************************************
*The function is to write data to the buffer,Called by buffer_management()
********************************************************************************/
struct ssd_info * insert2buffer(struct ssd_info *ssd, unsigned int lpn, int state, struct sub_request *sub, struct request *req)
{
	int write_back_count, flag = 0;                                  
	unsigned int sector_count, active_region_flag = 0, free_sector = 0;
	struct buffer_group *buffer_node = NULL, *pt, *new_node = NULL, key;
	struct sub_request *sub_req = NULL, *update = NULL;

	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	unsigned int add_size;

#ifdef DEBUG
	printf("enter insert2buffer,  current time:%I64u, lpn:%d, state:%d,\n", ssd->current_time, lpn, state);
#endif

	sector_count = size(state); 
	//sector_count = secno_num_sub_page;  //after 4KB aligning
	key.group = lpn;             
	buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->data_buffer, (TREE_NODE *)&key);   

	if (buffer_node == NULL)
	{
		free_sector = ssd->dram->data_buffer->max_buffer_sector - ssd->dram->data_buffer->buffer_sector_count;

		if (free_sector >= sector_count)
		{
			flag = 1;
		}
		if (flag == 0)
		{	
			write_back_count = sector_count - free_sector;
			while (write_back_count>0)
			{
				sub_req = NULL;
				sub_req_state = ssd->dram->data_buffer->buffer_tail->stored;
				//sub_req_size = size(ssd->dram->data_buffer->buffer_tail->stored);
				sub_req_size = secno_num_sub_page;   //after aligning
				sub_req_lpn = ssd->dram->data_buffer->buffer_tail->group;

				insert2_command_buffer(ssd, ssd->dram->data_command_buffer, sub_req_lpn, sub_req_size, sub_req_state, req, WRITE);  //deal with tail sub-request
				ssd->dram->data_buffer->write_miss_hit++;

				ssd->dram->data_buffer->buffer_sector_count = ssd->dram->data_buffer->buffer_sector_count - sub_req_size;
				
				pt = ssd->dram->data_buffer->buffer_tail;
				avlTreeDel(ssd->dram->data_buffer, (TREE_NODE *)pt);
				if (ssd->dram->data_buffer->buffer_head->LRU_link_next == NULL){
					ssd->dram->data_buffer->buffer_head = NULL;
					ssd->dram->data_buffer->buffer_tail = NULL;
				}
				else{
					ssd->dram->data_buffer->buffer_tail = ssd->dram->data_buffer->buffer_tail->LRU_link_pre;
					ssd->dram->data_buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(ssd->dram->data_buffer, (TREE_NODE *)pt);
				pt = NULL;

				write_back_count = write_back_count - sub_req_size; 
			}
		}

		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;
		new_node->dirty_clean = state;
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = ssd->dram->data_buffer->buffer_head;
		if (ssd->dram->data_buffer->buffer_head != NULL){
			ssd->dram->data_buffer->buffer_head->LRU_link_pre = new_node;
		}
		else{
			ssd->dram->data_buffer->buffer_tail = new_node;
		}
		ssd->dram->data_buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(ssd->dram->data_buffer, (TREE_NODE *)new_node);
		ssd->dram->data_buffer->buffer_sector_count += sector_count;
	}
	else
	{
		ssd->dram->data_buffer->write_hit++;
		if (req != NULL)
		{
			if (ssd->dram->data_buffer->buffer_head != buffer_node)
			{
				if (ssd->dram->data_buffer->buffer_tail == buffer_node)
				{
					ssd->dram->data_buffer->buffer_tail = buffer_node->LRU_link_pre;
					buffer_node->LRU_link_pre->LRU_link_next = NULL;
				}
				else if (buffer_node != ssd->dram->data_buffer->buffer_head)
				{
					buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
					buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
				}
				buffer_node->LRU_link_next = ssd->dram->data_buffer->buffer_head;
				ssd->dram->data_buffer->buffer_head->LRU_link_pre = buffer_node;
				buffer_node->LRU_link_pre = NULL;
				ssd->dram->data_buffer->buffer_head = buffer_node;
			}
			req->complete_lsn_count += size(state);                                       
		}
	}
	return ssd;
}


/*********************************************************************************************
*The no_buffer_distribute () function is processed when ssd has no dram，
*This is no need to read and write requests in the buffer inside the search, directly use the 
*creat_sub_request () function to create sub-request, and then deal with.
*********************************************************************************************/
struct ssd_info *no_buffer_distribute(struct ssd_info *ssd)
{
	return ssd;
}

/***********************************************************************************
*According to the status of each page to calculate the number of each need to deal 
*with the number of sub-pages, that is, a sub-request to deal with the number of pages
************************************************************************************/
unsigned int size(unsigned int stored)
{
	unsigned int i, total = 0, mask = 0x80000000;

#ifdef DEBUG
	printf("enter size\n");
#endif
	for (i = 1; i <= 32; i++)
	{
		if (stored & mask) total++;     
		stored <<= 1;
	}
#ifdef DEBUG
	printf("leave size\n");
#endif
	return total;
}

//Orderly insert to mapping command buffer 
struct ssd_info* insert2_mapping_command_buffer(struct ssd_info* ssd, unsigned int lpn, struct request* req)
{
	struct  buffer_group *front, *tmp;
	struct  buffer_group *new_node;

	new_node = (struct buffer_group*)malloc(sizeof(struct buffer_group));
	alloc_assert(new_node, "buffer_group_node");
	memset(new_node, 0, sizeof(struct buffer_group));
	
	
	new_node->LRU_link_next = NULL;
	new_node->LRU_link_pre = NULL;
	new_node->group = lpn;

	tmp = ssd->dram->mapping_command_buffer->buffer_head;
	front = tmp;
	if (tmp == NULL)
	{
		ssd->dram->mapping_command_buffer->buffer_head = new_node;
		ssd->dram->mapping_command_buffer->count++;
		return ssd;
	}

	while(tmp != NULL)
	{
		if (tmp->LRU_link_next == NULL)  //insert to tail
		{
			tmp->LRU_link_next = new_node;
			new_node->LRU_link_pre = tmp;
			ssd->dram->mapping_command_buffer->count++;
			break;
		}
		if (tmp->group < lpn)
		{
			front = tmp;
			tmp = tmp->LRU_link_next;
		}
		else if (tmp->group == lpn)
		{
			//no need to insert
			free(new_node);
			new_node = NULL;
			break;
		}
		else //insert the node
		{
			if (tmp ==  ssd->dram->mapping_command_buffer->buffer_head) //insert to the head  
			{
				new_node->LRU_link_next = tmp;
				tmp->LRU_link_pre = new_node;
				ssd->dram->mapping_command_buffer->buffer_head = new_node;
			}
			else
			{
				front->LRU_link_next = new_node;
				new_node->LRU_link_pre = front;
				new_node->LRU_link_next = tmp;
				tmp->LRU_link_pre = tmp;
			}
			ssd->dram->mapping_command_buffer->count++;

			if (ssd->dram->mapping_command_buffer->count == ssd->dram->mapping_command_buffer->max_command_buff_page)
			{
				//show_mapping_command_buffer(ssd);

				//trigger SMT dump
				smt_dump(ssd,req);
			}
			break;
		}
	}
	return ssd;
}

void show_mapping_command_buffer(struct ssd_info* ssd)
{
	struct  buffer_group* node;

	fprintf(ssd->smt, "******data**********\n");
	node = ssd->dram->mapping_command_buffer->buffer_head;
	while (node)
	{
		fprintf(ssd->smt, "%d ", node->group);
		node = node->LRU_link_next;
	}
	fprintf(ssd->smt, "\n");
	fflush(ssd->smt);
}

//SMT dump
struct ssd_info* smt_dump(struct ssd_info * ssd, struct request* req)
{
	struct  buffer_group *node, *tmp;
	unsigned int i;

	fprintf(ssd->smt, "*************Sorted Mapping Table***************\n");

	for (i = 0; i < ssd->dram->mapping_command_buffer->max_command_buff_page; i++)
	{
		node = ssd->dram->mapping_command_buffer->buffer_head;

		//write the reqeust  ->  fprintf mapping entries 
		fprintf(ssd->smt, "%d ", node->group);

	    //delete mapping entries from the mapping command  buffer
		tmp = node;
		node = node->LRU_link_next;
		ssd->dram->mapping_command_buffer->buffer_head = node;
		free(tmp);
		tmp = NULL;
		ssd->dram->mapping_command_buffer->count--;
	}
	fprintf(ssd->smt, "\n");
	fflush(ssd->smt);
	return ssd;
}

//insert to data commond buffer
struct ssd_info * insert2_command_buffer(struct ssd_info * ssd, struct buffer_info * command_buffer, unsigned int lpn, int size_count, unsigned int state, struct request * req, unsigned int operation)
{
	unsigned int i = 0, j = 0;
	unsigned int sub_req_state = 0, sub_req_size = 0, sub_req_lpn = 0;
	struct buffer_group *command_buffer_node = NULL, *pt, *new_node = NULL, key;
	struct sub_request *sub_req = NULL;
	int tmp;
	unsigned int loop, off;
	unsigned int lun_state, lun;

	key.group = lpn;
	command_buffer_node = (struct buffer_group*)avlTreeFind(command_buffer, (TREE_NODE *)&key);

	if (command_buffer_node == NULL)
	{
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;
		new_node->stored = state;
		new_node->dirty_clean = state;
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = command_buffer->buffer_head;
		if (command_buffer->buffer_head != NULL)
		{
			command_buffer->buffer_head->LRU_link_pre = new_node;
		}
		else
		{
			command_buffer->buffer_tail = new_node;
		}
		command_buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(command_buffer, (TREE_NODE *)new_node);
		command_buffer->command_buff_page++;

		if (command_buffer->command_buff_page >= command_buffer->max_command_buff_page)
		{
			loop = command_buffer->max_command_buff_page / ssd->parameter->subpage_page;

			//printf("begin to flush command_buffer\n");
			for (i = 0; i < loop; i++)
			{
				sub_req = (struct sub_request*)malloc(sizeof(struct sub_request));
				alloc_assert(sub_req, "sub_request");
				memset(sub_req, 0, sizeof(struct sub_request));
				sub_req->lun_count = 0;

				for (j = 0; j < ssd->parameter->subpage_page; j++)
				{
					lun = command_buffer->buffer_tail->group;
					lun_state = command_buffer->buffer_tail->stored;
					sub_req->luns[sub_req->lun_count] = lun;
					sub_req->lun_state[sub_req->lun_count++] = lun_state;		
				}
				create_sub_w_req(ssd, sub_req, req, USER_DATA);

				//delete the data node from command buffer
				pt = command_buffer->buffer_tail;
				avlTreeDel(command_buffer, (TREE_NODE *)pt);
				if (command_buffer->buffer_head->LRU_link_next == NULL){
					command_buffer->buffer_head = NULL;
					command_buffer->buffer_tail = NULL;
				}
				else{
					command_buffer->buffer_tail = command_buffer->buffer_tail->LRU_link_pre;
					command_buffer->buffer_tail->LRU_link_next = NULL;
				}
				pt->LRU_link_next = NULL;
				pt->LRU_link_pre = NULL;
				AVL_TREENODE_FREE(command_buffer, (TREE_NODE *)pt);
				pt = NULL;

				command_buffer->command_buff_page--;
 			}
			if (command_buffer->command_buff_page != 0)
			{
				printf("command buff flush failed\n");
				getchar();
			}
		}
	}
	else  
	{
		if (command_buffer->buffer_head != command_buffer_node)
		{
			if (command_buffer->buffer_tail == command_buffer_node)      //如果是最后一个节点  则交换最后两个
			{
				command_buffer_node->LRU_link_pre->LRU_link_next = NULL;
				command_buffer->buffer_tail = command_buffer_node->LRU_link_pre;
			}
			else
			{
				command_buffer_node->LRU_link_pre->LRU_link_next = command_buffer_node->LRU_link_next;     //如果是中间节点，则操作前一个和后一个节点的指针
				command_buffer_node->LRU_link_next->LRU_link_pre = command_buffer_node->LRU_link_pre;
			}
			command_buffer_node->LRU_link_next = command_buffer->buffer_head;              //提到队首
			command_buffer->buffer_head->LRU_link_pre = command_buffer_node;
			command_buffer_node->LRU_link_pre = NULL;
			command_buffer->buffer_head = command_buffer_node;
		}

		command_buffer_node->stored = command_buffer_node->stored | state;
		command_buffer_node->dirty_clean = command_buffer_node->dirty_clean | state;
	}

	return ssd;
}

//req -> write sub request  
Status update_read_request(struct ssd_info *ssd, unsigned int lpn, unsigned int state, struct sub_request *req, unsigned int commond_buffer_type)  //in this code, the state is for sector and maximal 32 bits
{
	struct sub_request *sub_r = NULL;
	struct sub_request * sub = NULL;
	struct channel_info * p_ch = NULL;
	struct local * loc = NULL;
	struct local * tem_loc = NULL;
	unsigned int flag = 0;
	int i= 0;
	unsigned int chan, chip, die, plane;
	unsigned int update_cnt;
	unsigned int off=0;
	struct sub_request* tmp_update;

	/**********************************************************
	     determine the update request count
		 generate the updates
		 note: data needed to read is the  in horizon data layout
   ******************************************************/

	unsigned int pn,state1;

	if (commond_buffer_type == DATA_COMMAND_BUFFER)
	{
		state1 = ssd->dram->map->map_entry[lpn].state;
		pn = ssd->dram->map->map_entry[lpn].pn;
	}
	else
	{
		pn = ssd->dram->tran_map->map_entry[lpn].pn;
		state1 = ssd->dram->tran_map->map_entry[lpn].state;
	}
	if (state1 == 0) //hit in reqeust queue
	{
		return SUCCESS;
		ssd->req_read_hit_cnt++;
		sub = (struct local *)malloc(sizeof(struct sub_request));
		tem_loc = (struct local *)malloc(sizeof(struct local));

		sub->next_node = NULL;
		sub->next_subs = NULL;

		if (sub == NULL)
		{
			return NULL;
		}
		sub->location = tem_loc;
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                       
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;

		sub->update_read_flag = 1;

		tmp_update = req->update;
		if (tmp_update == NULL)
		{
			req->update = sub;
			return SUCCESS;
		}
		while (tmp_update->next_node != NULL)
		{
			tmp_update = tmp_update->next_node;
		}
		tmp_update->next_node = sub;
		return SUCCESS;
	}



	loc = find_location(ssd, pn);
	sub = (struct local *)malloc(sizeof(struct sub_request));
	tem_loc = (struct local *)malloc(sizeof(struct local));
	sub->next_node = NULL;
	sub->next_subs = NULL;
	//sub->update = NULL;
	sub->read_flag = UPDATE_READ;
	tem_loc->channel = loc->channel;
	tem_loc->chip = loc->chip;
	tem_loc->die = loc->die;
	tem_loc->plane = loc->plane;
	tem_loc->block = loc->block;
	tem_loc->page = loc->page;
	sub->location = tem_loc;
	sub->lpn = ssd->channel_head[tem_loc->channel].chip_head[tem_loc->chip].die_head[tem_loc->die].plane_head[tem_loc->plane].blk_head[tem_loc->block].page_head[tem_loc->page].lpn;
	sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block,tem_loc->page);
	sub->state = ssd->channel_head[tem_loc->channel].chip_head[tem_loc->chip].die_head[tem_loc->die].plane_head[tem_loc->plane].blk_head[tem_loc->block].page_head[tem_loc->page].valid_state;
	sub->size = size(ssd->channel_head[tem_loc->channel].chip_head[tem_loc->chip].die_head[tem_loc->die].plane_head[tem_loc->plane].blk_head[tem_loc->block].page_head[tem_loc->page].valid_state);
	
	creat_one_read_sub_req(ssd, sub);  //insert into channel read  req queue

	sub->update_read_flag = 1;
	tmp_update = req->update;
	if (tmp_update == NULL)
	{
		req->update = sub;
		return SUCCESS;
	}
	while (tmp_update->next_node != NULL)
	{
		tmp_update = tmp_update->next_node;
	}
	tmp_update->next_node = sub;

	free(loc);
	loc = NULL;
	return SUCCESS;
}

Status read_reqeust(struct ssd_info *ssd, unsigned int lpn, struct request *req, unsigned int state,unsigned int data_type)
{
	struct sub_request* sub = NULL;
	struct local * loc = NULL;
	struct local *tem_loc = NULL;
	unsigned int data_size;
	unsigned int off = 0;
	unsigned int read_size;

	unsigned int pn;
	
	if (data_type == USER_DATA)  
		pn = ssd->dram->map->map_entry[lpn].pn;

	if (pn == -1) //hit in sub reqeust queue
	{
		ssd->req_read_hit_cnt++;
		sub = (struct local *)malloc(sizeof(struct sub_request));
		tem_loc = (struct local *)malloc(sizeof(struct local));

		sub->next_node = NULL;
		sub->next_subs = NULL;

		if (sub == NULL)
		{
			return FAILURE;
		}
		sub->location = tem_loc;
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                         //置为完成状态
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;
		sub->update = NULL;
		if (req == NULL)  //request is NULL means update reqd in this version. sub->update
		{
			req = sub;
		}
		else
		{
			sub->next_subs = req->subs;
			req->subs = sub;
			sub->total_request = req;
		}
		return SUCCESS;
	}
	else
	{
		loc = find_location(ssd, pn);
		sub = (struct local*)malloc(sizeof(struct sub_request));
		tem_loc = (struct local*)malloc(sizeof(struct local));

		sub->next_node = NULL;
		sub->next_subs = NULL;

		if (sub == NULL)
		{
			return NULL;
		}
		sub->next_node = NULL;
		sub->next_subs = NULL;
		sub->read_flag = REQ_READ;
		sub->state = state;
		sub->size = size(state);
		sub->update = NULL;

		if (req == NULL)  //request is NULL means update reqd in this version. sub->update
		{
			req = sub;
		}
		else
		{
			sub->next_subs = req->subs;
			req->subs = sub;
			sub->total_request = req;
		}

		tem_loc->channel = loc->channel;
		tem_loc->chip = loc->chip;
		tem_loc->die = loc->die;
		tem_loc->plane = loc->plane;
		tem_loc->block = loc->block;
		tem_loc->page = loc->page;
		sub->location = tem_loc;
		sub->lpn = ssd->channel_head[tem_loc->channel].chip_head[tem_loc->chip].die_head[tem_loc->die].plane_head[tem_loc->plane].blk_head[tem_loc->block].page_head[tem_loc->page].lpn;
		sub->ppn = find_ppn(ssd, tem_loc->channel, tem_loc->chip, tem_loc->die, tem_loc->plane, tem_loc->block, tem_loc->page);
		creat_one_read_sub_req(ssd, sub);  //insert into channel read  req queue
		free(loc);
		return SUCCESS;
	}
}

Status create_sub_w_req(struct ssd_info* ssd, struct sub_request* sub, struct request* req, unsigned int data_type)
{
	unsigned int i; 
	unsigned int lun,lun_state;
	struct sub_request* sub_w = NULL, *tmp = NULL;


	if (sub->lun_count != ssd->parameter->subpage_page)
	{
		printf("Cannot Create Sub Write Request\n");
	}

	sub->next_node = NULL;
	sub->next_subs = NULL;
	sub->operation = WRITE;
	sub->location = (struct local*)malloc(sizeof(struct local));
	alloc_assert(sub->location, "sub->location");
	memset(sub->location, 0, sizeof(struct local));
	sub->current_state = SR_WAIT;
	sub->current_time = ssd->current_time;
	sub->size = secno_num_per_page;
	sub->begin_time = ssd->current_time;

	if (req != NULL)
	{
		sub->next_subs = req->subs;
		req->subs = sub;
		sub->total_request = req;
	}

	//apply for free page 
				//apply for free super page
	if (ssd->open_sb[data_type]->next_wr_page >= ssd->parameter->page_block) //no free superpage in the superblock
		find_active_superblock(ssd, data_type);

	//allocate free page 
	ssd->open_sb[data_type]->pg_off = (ssd->open_sb[data_type]->pg_off + 1) % ssd->sb_pool[data_type].blk_cnt;
	sub->location->channel = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].channel;
	sub->location->chip = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].chip;
	sub->location->die = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].die;
	sub->location->plane = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].plane;
	sub->location->block = ssd->open_sb[data_type]->pos[ssd->open_sb[data_type]->pg_off].block;
	sub->location->page = ssd->open_sb[data_type]->next_wr_page;

	if (ssd->open_sb[data_type]->pg_off == ssd->sb_pool[data_type].blk_cnt - 1)
		ssd->open_sb[data_type]->next_wr_page++;

	sub->ppn = find_ppn(ssd, sub->location->channel, sub->location->chip, sub->location->die, sub->location->plane, sub->location->block, sub->location->page);

	//handle update write 
	for (i = 0; i < sub->lun_count; i++)
	{
		lun = sub->luns[i];
		lun_state = sub->lun_state[i];

		if (ssd->dram->map->map_entry[lun].state != 0)  // maybe update read
		{
			if (size(lun_state) != secno_num_sub_page)  //patitial write
			{
				update_read_request(ssd, lun, lun_state, sub, USER_DATA);
			}
		}

		//insert into sub request queue
		sub_w = ssd->channel_head[sub->location->channel].subs_w_head;
		if (ssd->channel_head[sub->location->channel].subs_w_tail != NULL)
		{
			ssd->channel_head[sub->location->channel].subs_w_tail->next_node = sub;
			ssd->channel_head[sub->location->channel].subs_w_tail = sub;
		}
		else
		{
			ssd->channel_head[sub->location->channel].subs_w_head = sub;
			ssd->channel_head[sub->location->channel].subs_w_tail = sub;
		}
	}

	return SUCCESS;
}

/**************************************************************
this function is to create sub_request based on lpn, size, state
****************************************************************/
struct sub_request * creat_sub_request(struct ssd_info * ssd, unsigned int lpn, int size, unsigned int state, struct request * req, unsigned int operation,unsigned int command_type)
{
	struct sub_request* sub = NULL, *sub_r = NULL, *update = NULL, *sub_w = NULL, *tmp_sub_w = NULL;
	struct channel_info * p_ch = NULL;
	struct local * loc = NULL;
	unsigned int flag = 0;
	int i;
	unsigned int chan, chip, die, plane;
	unsigned int map_size, map_state,off;
	unsigned int map_size2, map_state2, off2, state22;
	int t;

	unsigned int bits = 0;

	if (operation == READ)
	{
		read_reqeust(ssd, lpn, req,state,command_type);
	}
	else if (operation == WRITE)
	{
		sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*申请一个子请求的结构*/
		alloc_assert(sub, "sub_request");
		memset(sub, 0, sizeof(struct sub_request));

		if (sub == NULL)
		{
			return NULL;
		}
		sub->location = NULL;
		sub->next_node = NULL;
		sub->next_subs = NULL;
		//sub->update = NULL;
		sub->lpn = lpn;

		if (req != NULL)
		{
			sub->next_subs = req->subs;
			req->subs = sub;
			sub->total_request = req;
		}
		
		sub->ppn = 0;
		sub->operation = WRITE;
		sub->location = (struct local *)malloc(sizeof(struct local));
		alloc_assert(sub->location, "sub->location");
		memset(sub->location, 0, sizeof(struct local));

		//sub->update = (struct sub_request **)malloc(MAX_SUPERBLOCK_SISE*sizeof(struct sub_request *));
		sub->current_state = SR_WAIT;
		sub->current_time = ssd->current_time;
		sub->lpn = lpn;
		sub->size = size;
		sub->state = state;
		sub->begin_time = ssd->current_time;

		if (ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION)
		{
			//apply for free super page
			if (ssd->open_sb[command_type]->next_wr_page >= ssd->parameter->page_block) //no free superpage in the superblock
				find_active_superblock(ssd,command_type);

			//allocate free page 
			ssd->open_sb[command_type]->pg_off = (ssd->open_sb[command_type]->pg_off + 1) % ssd->sb_pool[command_type].blk_cnt;
			sub->location->channel = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].channel;
			sub->location->chip = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].chip;
			sub->location->die = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].die;
			sub->location->plane = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].plane;
			sub->location->block = ssd->open_sb[command_type]->pos[ssd->open_sb[command_type]->pg_off].block;
			sub->location->page = ssd->open_sb[command_type]->next_wr_page;

			if (ssd->open_sb[command_type]->pg_off == ssd->sb_pool[command_type].blk_cnt - 1)
				ssd->open_sb[command_type]->next_wr_page++;


			sub->ppn = find_ppn(ssd, sub->location->channel, sub->location->chip, sub->location->die, sub->location->plane, sub->location->block, sub->location->page);

			if (ssd->dram->map->map_entry[lpn].state != 0)//update write 
			{
				if (size != secno_num_per_page)  //not full page 
				{	
					state22 = 0;
					for (t = 0; t < ssd->parameter->subpage_page; t++)
					{
						if (GET_BIT(state, t) == 0)
						{
							state22 = SET_VALID(state22, t);
							bits++;
						}
					}
					//create a new read operation
					update_read_request(ssd, lpn, state22, sub, DATA_COMMAND_BUFFER);
				}
			}
			sub_w = ssd->channel_head[sub->location->channel].subs_w_head;

			// can hit write request queue?? 
			while (sub_w != NULL)
			{
				if (sub_w->ppn == sub->ppn)  //no possibility to write into the same physical position
				{
					printf("error: write into the same physical address\n");
					getchar();
				}
				sub_w = sub_w->next_node;
			}

			if (ssd->channel_head[sub->location->channel].subs_w_tail != NULL)
			{
				ssd->channel_head[sub->location->channel].subs_w_tail->next_node = sub;
				ssd->channel_head[sub->location->channel].subs_w_tail = sub;
			}
			else
			{
				ssd->channel_head[sub->location->channel].subs_w_head = sub;
				ssd->channel_head[sub->location->channel].subs_w_tail = sub;
			}

		}
	}
	else
	{
		free(sub->location);
		sub->location = NULL;
		free(sub);
		sub = NULL;
		printf("\nERROR ! Unexpected command.\n");
		return NULL;
	}

	return sub;
}

Status creat_one_read_sub_req(struct ssd_info *ssd, struct sub_request* sub)
{
	unsigned int lpn,flag;
	struct channel_info * p_ch = NULL;
	struct local *loc = NULL;
	struct sub_request* sub_r;

	lpn = sub->lpn;
	sub->begin_time = ssd->current_time;
	sub->current_state = SR_WAIT;
	sub->current_time = 0x7fffffffffffffff;
	sub->next_state = SR_R_C_A_TRANSFER;
	sub->next_state_predict_time = 0x7fffffffffffffff;
	//sub->size = size;                                                    
	//sub->update_read_flag = 0;
	sub->suspend_req_flag = NORMAL_TYPE;

	//sub->ppn = sub->ppn;
	//ssd->dram->map->map_entry[lpn].pn;
	loc = sub->location;
	p_ch = &ssd->channel_head[loc->channel];
	
	sub->operation = READ;
	sub_r = ssd->channel_head[loc->channel].subs_r_head;

	flag = 0;
	while (sub_r != NULL)
	{
		if (sub_r->ppn == sub->ppn)                          
		{
			if (sub->read_flag == REQ_READ)
				ssd->req_read_hit_cnt++;
			if (sub->read_flag == GC_READ)
				ssd->gc_read_hit_cnt++;
			if (sub->read_flag == UPDATE_READ)
				ssd->update_read_hit_cnt++;
			flag = 1;
			break;
		}
		sub_r = sub_r->next_node;
	}
	
	if (flag == 0)          
	{
		ssd->channel_head[loc->channel].chip_head[loc->chip].die_head[loc->die].read_cnt++;
		if (ssd->channel_head[loc->channel].subs_r_tail != NULL)
		{
			ssd->channel_head[loc->channel].subs_r_tail->next_node = sub;          //sub挂在子请求队列最后
			ssd->channel_head[loc->channel].subs_r_tail = sub;
		}
		else
		{
			ssd->channel_head[loc->channel].subs_r_head = sub;
			ssd->channel_head[loc->channel].subs_r_tail = sub;
		}
	}
	else
	{
		sub->current_state = SR_R_DATA_TRANSFER;
		sub->current_time = ssd->current_time;
		sub->next_state = SR_COMPLETE;                                         //置为完成状态
		sub->next_state_predict_time = ssd->current_time + 1000;
		sub->complete_time = ssd->current_time + 1000;
	}
	return SUCCESS;
}