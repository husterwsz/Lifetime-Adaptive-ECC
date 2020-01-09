#define _CRTDBG_MAP_ALLOC

#include <stdlib.h>
#include <crtdbg.h>

#include "flash.h"
#include "ssd.h"
#include "initialize.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"

extern int secno_num_per_page, secno_num_sub_page;

//deal with read requests at one time
Status service_2_read(struct ssd_info* ssd, unsigned int channel)
{
	unsigned int chip, subs_count, i;
	unsigned int aim_die, plane_round, plane0, plane1;
	unsigned int transfer_size = 0;
	unsigned int mp_flag;
	struct sub_req* p_sub0, * p_sub1, * d_sub;
	struct sub_request** sub_r_request = NULL;
	unsigned int MP_flag;
	unsigned int max_sub_num;

	subs_count = 0;
	max_sub_num = ssd->parameter->chip_channel[channel]*ssd->parameter->die_chip * ssd->parameter->plane_die * PAGE_INDEX;

	sub_r_request = (struct sub_request**)malloc(max_sub_num * sizeof(struct sub_request*));
	alloc_assert(sub_r_request, "sub_r_request");


	for (i = 0; i < max_sub_num; i++)
		sub_r_request[i] = NULL;

	for (chip = 0; chip < ssd->channel_head[channel].chip; chip++)
	{
		if ((ssd->channel_head[channel].chip_head[chip].current_state == CHIP_IDLE) ||
			((ssd->channel_head[channel].chip_head[chip].next_state == CHIP_IDLE) &&
			(ssd->channel_head[channel].chip_head[chip].next_state_predict_time <= ssd->current_time)))
		{
			for (aim_die = 0; aim_die < ssd->parameter->die_chip; aim_die++)
			{
				MP_flag = 0;
				for (plane_round = 0; plane_round < ssd->parameter->plane_die / 2; plane_round++)
				{
					plane0 = plane_round * 2;
					plane1 = plane_round * 2 + 1;
					p_sub0 = get_first_plane_read_request(ssd, channel, chip, aim_die, plane0);
					p_sub1 = get_first_plane_read_request(ssd, channel, chip, aim_die, plane1);

					//judge whether multiple-read can be carried out 
					mp_flag = IS_Multi_Plane(ssd, p_sub0, p_sub1);

					if (mp_flag == 1) //multi plane read
					{
						ssd->m_plane_read_count++;
						sub_r_request[subs_count++] = p_sub0;
						sub_r_request[subs_count++] = p_sub1;
						Multi_Plane_Read(ssd, p_sub0, p_sub1);
						MP_flag = 1;
					}
				}
				if (MP_flag == 0)
				{
					d_sub = get_first_die_read_request(ssd, channel, chip, aim_die);

					if (d_sub != NULL)
					{
						sub_r_request[subs_count++] = d_sub;
						Read(ssd, d_sub);
					}
				}
			}
		}
	}

	if (subs_count == 0)
	{
		for (i = 0; i < max_sub_num; i++)
		{
			sub_r_request[i] = NULL;
		}
		free(sub_r_request);
		sub_r_request = NULL;
		return ssd;
	}

	//compute_read_serve_time(ssd,sub_r_request)
	compute_read_serve_time(ssd, channel, chip, sub_r_request, subs_count);

	//freet the malloc 
	for (i = 0; i < max_sub_num; i++)
	{
		sub_r_request[i] = NULL;
	}
	free(sub_r_request);
	sub_r_request = NULL;

	return SUCCESS;
}

#if 0
//deal with read requests at one time
Status service_2_read(struct ssd_info* ssd, unsigned int channel)
{
	unsigned int chip, subs_count, i;
	unsigned int aim_die;
	unsigned int transfer_size = 0;
	unsigned int lpn, ppn;
	unsigned int chan0, chip0, die0, plane0, block0, page0;
	double tmp_time;
	struct loc* location;
	location = (struct loc*)malloc(sizeof(struct loc*));


	struct sub_request** sub_r_request = NULL;
	sub_r_request = (struct sub_request**)malloc((ssd->parameter->die_chip * PAGE_INDEX) * sizeof(struct sub_request*));
	alloc_assert(sub_r_request, "sub_r_request");
	for (i = 0; i < (ssd->parameter->die_chip * ssd->parameter->plane_die*PAGE_INDEX); i++)
		sub_r_request[i] = NULL;

	for (chip = 0; chip < ssd->channel_head[channel].chip; chip++)
	{
		if ((ssd->channel_head[channel].chip_head[chip].current_state == CHIP_IDLE) ||
			((ssd->channel_head[channel].chip_head[chip].next_state == CHIP_IDLE) &&
			(ssd->channel_head[channel].chip_head[chip].next_state_predict_time <= ssd->current_time)))
		{
			for (aim_die = 0; aim_die < ssd->parameter->die_chip; aim_die++)
			{
				//find read request
				subs_count = find_read_sub_request(ssd, channel, chip, aim_die, sub_r_request, SR_WAIT, NORMAL);

				//complte the read requests
				for (i = 0; i < subs_count; i++)
				{
					lpn = sub_r_request[i]->lpn;
					ppn = sub_r_request[i]->ppn;

					//read 
					if (NAND_read(ssd, sub_r_request[i]) == FAILURE)
					{
						//printf("ERROR! read error!\n");
						//getchar();
					}

					switch (sub_r_request[i]->read_flag)
					{
					case REQ_READ:
						ssd->req_read_count++;
						break;
					case UPDATE_READ:
						ssd->update_read_count++;
						break;
					default:
						break;
					}
					ssd->channel_head[channel].chip_head[chip].die_head[aim_die].read_cnt--;
					//sub_r_request[i]->size = size(ssd->dram->map->map_entry[lpn].state);
					sub_r_request[i]->current_state = SR_R_DATA_TRANSFER;
					sub_r_request[i]->next_state = SR_COMPLETE;
					transfer_size += sub_r_request[i]->size;
					sub_r_request[i]->complete_time = ssd->current_time + 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR + (transfer_size * ssd->parameter->subpage_capacity) * ssd->parameter->time_characteristics.tRC;
					sub_r_request[i]->next_state_predict_time = sub_r_request[i]->complete_time;


				}
			}
			if (transfer_size > 0)
			{
				ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->current_time + 7 * ssd->parameter->time_characteristics.tWC + ssd->parameter->time_characteristics.tR;
				tmp_time = ssd->channel_head[channel].chip_head[chip].next_state_predict_time + (transfer_size * ssd->parameter->subpage_capacity) * ssd->parameter->time_characteristics.tRC;
				ssd->channel_head[channel].next_state_predict_time = (tmp_time > ssd->channel_head[channel].next_state_predict_time) ? tmp_time : ssd->channel_head[channel].next_state_predict_time;
			}
		}
	}

	//freet the malloc 
	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		sub_r_request[i] = NULL;
	free(sub_r_request);
	sub_r_request = NULL;
	free(location);

	return SUCCESS;
}


/**************************************************************************************
*Function function is given in the channel, chip, die above looking for reading requests
*The request for this child ppn corresponds to the ppn of the corresponding plane's register
*****************************************************************************************/
unsigned int find_read_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, struct sub_request ** subs, unsigned int state, unsigned int command)
{
	struct sub_request * sub = NULL;
	unsigned int add_reg, i, j = 0;

	for (i = 0; i < (ssd->parameter->plane_die * PAGE_INDEX); i++)
		subs[i] = NULL;
	
	j = 0;
	sub = ssd->channel_head[channel].subs_r_head;
	while (sub != NULL)
	{
		if (sub->location->chip == chip && sub->location->die == die)
		{
			if (sub->current_state == state)
			{
				if (command == MUTLI_PLANE)
				{
					subs[j++] = sub;
					if (j == ssd->parameter->plane_die)
						break;
				}
				else if (command == NORMAL)
				{
					if ((sub->oneshot_mutliplane_flag != 1) && (sub->oneshot_flag != 1) && (sub->mutliplane_flag != 1))
					{
						subs[j++] = sub;
					}
					if (j == 1)
						break; 
				}
			}
		}
		sub = sub->next_node;
	}
 

	if (j > (ssd->parameter->plane_die * PAGE_INDEX))
	{
		printf("error,beyong plane_die* PAGE_INDEX\n");
		getchar();
	}

	return j;
}
#endif

/*********************************************************************************************
* function that specifically serves a read request
*1£¬Only when the current state of the sub request is SR_R_C_A_TRANSFER
*2£¬The current state of the read request is SR_COMPLETE or the next state is SR_COMPLETE and 
*the next state arrives less than the current time
**********************************************************************************************/
Status services_2_r_complete(struct ssd_info * ssd)
{
	unsigned int i = 0;
	struct sub_request * sub = NULL, *p = NULL;
	
	for (i = 0; i<ssd->parameter->channel_number; i++)                                       /*This loop does not require the channel time, when the read request is completed, it will be removed from the channel queue*/
	{
		sub = ssd->channel_head[i].subs_r_head;
		p = NULL;
		while (sub != NULL)
		{
			if ((sub->current_state == SR_COMPLETE) || sub->next_state == SR_COMPLETE )
			{
				if (sub != ssd->channel_head[i].subs_r_head)                         
				{
					if (sub == ssd->channel_head[i].subs_r_tail)
					{
						ssd->channel_head[i].subs_r_tail = p;
						p->next_node = NULL;
					}
					else
					{
						p->next_node = sub->next_node;
						sub = p->next_node;
					}
				}
				else
				{
					if (ssd->channel_head[i].subs_r_head != ssd->channel_head[i].subs_r_tail)
					{
						ssd->channel_head[i].subs_r_head = sub->next_node;
						sub = sub->next_node;
						p = NULL;
					}
					else
					{
						ssd->channel_head[i].subs_r_head = NULL;
						ssd->channel_head[i].subs_r_tail = NULL;
						break;
					}
				}
			}
			else
			{
				p = sub;
				sub = sub->next_node;
			}
		}
	}
	return SUCCESS;
}

/****************************************
Write the request function of the request
*****************************************/
Status services_2_write(struct ssd_info * ssd, unsigned int channel)
{
	int j = 0,i = 0;
	unsigned int chip_token = 0;
	struct sub_request *sub = NULL;

	/************************************************************************************************************************
	*Because it is dynamic allocation, all write requests hanging in ssd-> subs_w_head, that is, do not know which allocation before writing on the channel
	*************************************************************************************************************************/
	if (ssd->subs_w_head != NULL || ssd->channel_head[channel].subs_w_head != NULL)
	{
		if (ssd->parameter->allocation_scheme == SUPERBLOCK_ALLOCATION)
		{
			for (j = 0; j < ssd->channel_head[channel].chip; j++)
			{
				if (ssd->channel_head[channel].subs_w_head == NULL)
					continue;

				if ((ssd->channel_head[channel].chip_head[j].current_state == CHIP_IDLE) || ((ssd->channel_head[channel].chip_head[j].next_state == CHIP_IDLE) && (ssd->channel_head[channel].chip_head[j].next_state_predict_time <= ssd->current_time)))
				{
					if (dynamic_advanced_process(ssd, channel, j) == NULL)
						ssd->channel_head[channel].channel_busy_flag = 0;
					else
						ssd->channel_head[channel].channel_busy_flag = 1;
				}
	
			}
		}
	}
	else
	{
		ssd->channel_head[channel].channel_busy_flag = 0;
	}
	return SUCCESS;
}

//get the first read request from the given plane
struct sub_request* get_first_plane_read_request(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane)
{
	struct sub_request* temp;
	temp = NULL;

	temp = ssd->channel_head[channel].subs_r_head;

	while (temp != NULL)
	{
		if (temp->current_state != SR_WAIT)
		{
			temp = temp->next_node;
			continue;
		}
		if (temp->location->channel == channel && temp->location->chip == chip && temp->location->die == die && temp->location->plane == plane) //the first request allocated to the given plane
		{
			return temp;
		}
		temp = temp->next_node;
	}
	return NULL;
}


//get the first read request from the given die
struct sub_request* get_first_die_read_request(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die)
{
	struct sub_request* temp;
	temp = NULL;

	temp = ssd->channel_head[channel].subs_r_head;

	while (temp != NULL)
	{
		if (temp->current_state != SR_WAIT)
		{
			temp = temp->next_node;
			continue;
		}
		if (temp->location->channel == channel && temp->location->chip == chip && temp->location->die == die) //the first request allocated to the given plane
		{
			return temp;
		}
		temp = temp->next_node;
	}
	return NULL;
}

//get the first request from the given plane
struct sub_request* get_first_plane_write_request(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane)
{
	struct sub_request* temp;
	temp = NULL;
	
	temp = ssd->channel_head[channel].subs_w_head;

	while (temp != NULL)
	{
		if (temp->current_state != SR_WAIT)
		{
			temp = temp->next_node;
			continue;
		}
		if (temp->location->channel == channel && temp->location->chip == chip && temp->location->die == die && temp->location->plane == plane) //the first request allocatde to the given plane
		{
			if(temp->update == NULL)
				return temp;
			else
			{
				/*
				if (temp->update->current_state == SR_COMPLETE || (temp->update->next_state == SR_COMPLETE && temp->update->next_state_predict_time<=ssd->current_time))
					return temp;
				else
					return NULL;
				*/
				if (IS_Update_Done(ssd, temp))
					return temp;
				else
					return	NULL;			
			}
		}
		temp = temp->next_node;
	}
	return NULL;
}

int IS_Update_Done(struct ssd_info* ssd, struct sub_request* sub)
{
	struct sub_request* tmp_update;
	if (sub == NULL)
		return TRUE;
	if (sub->update == NULL)
		return TRUE;

	tmp_update = sub->update;
	while (tmp_update)
	{
		if (tmp_update->current_state != SR_COMPLETE && tmp_update->next_state != SR_COMPLETE)
			return FALSE;
		tmp_update = tmp_update->next_node;
	}
	return TRUE;
}

//get the first request from the given die
struct sub_request* get_first_die_write_request(struct ssd_info* ssd, unsigned int channel, unsigned int chip, unsigned int die)
{
	struct sub_request* temp;
	temp = NULL;

	temp = ssd->channel_head[channel].subs_w_head;

	while (temp != NULL)
	{
		if(temp->current_state != SR_WAIT)
		{
			temp = temp->next_node;
			continue;
		}
		if (temp->location->channel == channel && temp->location->chip == chip && temp->location->die == die)
		{
			if (temp->update == NULL)
				return temp;
			else
			{
				if (IS_Update_Done(ssd, temp))
					return temp;
				else
					return	NULL;	
			}
		}
		temp = temp->next_node;
	}
	return NULL;
}

//judge whether reqeusts can be performed in multi-plane 
int IS_Multi_Plane(struct ssd_info* ssd, struct sub_request* sub0, struct sub_request* sub1)
{
	int flag;
	if (sub0 == NULL || sub1 == NULL)
		return 0;
	return (sub0->location->page == sub1->location->page);
}


//perform requests in multi-plane
Status Multi_Plane_Read(struct ssd_info* ssd, struct sub_request* sub0, struct sub_request* sub1)
{
	//read the data 
	if (NAND_multi_plane_read(ssd, sub0, sub1) == FAILURE)
	{
		printf("Read Failure\n");
		getchar();
		return FAILURE;
	}

	//modify the state 
	Update_read_req(ssd, sub0, sub1);
}

//update the state for completed read request
Status Update_read_req(struct ssd_info* ssd, struct sub_request* sub)
{
	unsigned int channel, chip, die, plane, block, page;
	
	channel = sub->location->channel;
	chip = sub->location->chip;
	die = sub->location->die;
	
	ssd->channel_head[channel].chip_head[chip].die_head[die].read_cnt--;
	sub->current_state = SR_R_DATA_TRANSFER;
	sub->next_state = SR_COMPLETE;
}


//perform requests in multi-plane
Status Multi_Plane_Write(struct ssd_info* ssd, struct sub_request* sub0, struct sub_request* sub1)
{
	//write the data 
	if (NAND_multi_plane_program(ssd, sub0, sub1) == FAILURE)
	{
		printf("Program Failure\n");
		getchar();
		return FAILURE;
	}

	//build the mapping table 
	Add_mapping_entry(ssd, sub0);
	Add_mapping_entry(ssd, sub1);
}

//conduct request 
Status Write(struct ssd_info* ssd, struct sub_request* sub)
{
	if (NAND_program(ssd, sub) == FAILURE)
	{
		printf("Program Failure\n");
		getchar();
		return FAILURE;
	}

	//build the mapping table 
	Add_mapping_entry(ssd, sub);
}

//conduct request 
Status Read(struct ssd_info* ssd, struct sub_request* sub)
{
	if (NAND_read(ssd, sub) == FAILURE)
	{
		printf("Program Failure\n");
		getchar();
		return FAILURE;
	}

	//update the read request
	Update_read_req(ssd, sub);
}

Status Add_mapping_entry(struct ssd_info* ssd, struct sub_request* sub)
{
	unsigned int channel, chip, die, plane, block, page;
	unsigned int lpn,ppn,blk_type;
	struct local* location;

	//form mapping entry
	channel = sub->location->channel;
	chip = sub->location->chip;
	die = sub->location->die;
	plane = sub->location->plane;
	block = sub->location->block;
	page = sub->location->page;

	blk_type = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].block_type;

	if (blk_type == DATA_BLK)
	{
		for (int i = 0; i < ssd->parameter->subpage_page; i++)
		{
			lpn = sub->luns[i];

			if (ssd->dram->map->map_entry[lpn].state == 0)                                       /*this is the first logical page*/
			{
				ssd->dram->map->map_entry[lpn].pn = find_ppn(ssd, channel, chip, die, plane, block, page);
				ssd->dram->map->map_entry[lpn].state = sub->lun_state[i];
			}
			else                                                                            /*This logical page has been updated, and the original page needs to be invalidated*/
			{


				ppn = ssd->dram->map->map_entry[lpn].pn;
				location = find_location(ssd, ppn);

				if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn != lpn)
				{
					printf("\nError in get_ppn()\n");
					getchar();
				}

				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state = 0;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state = 0;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn = -1;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;

				free(location);
				location = NULL;
				ssd->dram->map->map_entry[lpn].pn = find_ppn(ssd, channel, chip, die, plane, block, page);
				ssd->dram->map->map_entry[lpn].state = (ssd->dram->map->map_entry[lpn].state | sub->lun_state[i]);
			}
			sub->ppn = ssd->dram->map->map_entry[lpn].pn;

		}
	}
	return SUCCESS;
}

/****************************************************************************************************************************
*When ssd supports advanced commands, the function of this function is to deal with high-level command write request
*According to the number of requests, decide which type of advanced command to choose (this function only deal with write requests, 
*read requests have been assigned to each channel, so the implementation of the election between the corresponding command)
*****************************************************************************************************************************/
struct ssd_info *dynamic_advanced_process(struct ssd_info *ssd, unsigned int channel, unsigned int chip)
{
	unsigned int subs_count = 0;
	unsigned int update_count = 0;                                                                                                                     /*record which plane has sub request in static allocation*/
	struct sub_request *sub = NULL, *p = NULL;
	struct sub_request ** subs = NULL;
	struct sub_request * p_sub0 = NULL, * p_sub1 = NULL, * d_sub = NULL;
	unsigned int max_sub_num = 0, aim_subs_count;
	unsigned int die_token = 0, plane_token = 0, plane0, plane1, mp_flag;
	unsigned int mask = 0x00000001;
	unsigned int i = 0, j = 0, k = 0;
	unsigned int die,plane_round;
	unsigned int MP_flag;  //max 2 multiple plane operation in modern SSD


	max_sub_num = (ssd->parameter->die_chip)*(ssd->parameter->plane_die)*PAGE_INDEX;
	subs = (struct sub_request **)malloc(max_sub_num*sizeof(struct sub_request *));
	alloc_assert(subs, "sub_request");
	update_count = 0;
	for (i = 0; i < max_sub_num; i++)
		subs[i] = NULL;  
	
	sub = ssd->channel_head[channel].subs_w_head;

	//perform requests in die-level round robin : interleave requests
	for (die = 0; die < ssd->parameter->die_chip; die++) 
	{
		MP_flag = 0;

		//perform request in multi-plane whenever possible (plane 0 & plane 1 and plane 2 & plane 3)
		for (plane_round = 0; plane_round < ssd->parameter->plane_die / 2; plane_round++)
		{
			//get the first request from each plane
			plane0 = plane_round * 2;
			plane1 = plane_round * 2 + 1;
			p_sub0 = get_first_plane_write_request(ssd, channel, chip, die, plane0);
			p_sub1 = get_first_plane_write_request(ssd, channel, chip, die, plane1);

			//judge whether sub requests can perform in multi-plane program 
			mp_flag = IS_Multi_Plane(ssd, p_sub0, p_sub1);

			if (mp_flag == 1) //multi-plane
			{
				ssd->m_plane_prog_count++;
				subs[subs_count++] = p_sub0;
				subs[subs_count++] = p_sub1;
				Multi_Plane_Write(ssd, p_sub0, p_sub1);
				MP_flag = 1;
			}			
		}

		if (MP_flag==0) // normal write
		{
			d_sub = get_first_die_write_request(ssd, channel, chip, die);
		
			if (d_sub == NULL)  // maybe p_sub0 == NULL and p_sub1 != NULL  or   p_sub1 == NULL and p_sub0 != NULL
			{
				if (p_sub0 != NULL)
					d_sub = p_sub0;
				if (p_sub1 != NULL)
					d_sub = p_sub1;
			}

			if (d_sub != NULL)
			{
				subs[subs_count++] = d_sub;
				Write(ssd, d_sub);
			}
		}
	}

	if (subs_count == 0)
	{
		for (i = 0; i < max_sub_num; i++)
		{
			subs[i] = NULL;
		}
		free(subs);
		subs = NULL;
		return ssd;
	}

	//compute server time 
	compute_serve_time(ssd, channel, chip, subs, subs_count);

	for (i = 0; i < max_sub_num; i++)
	{
		subs[i] = NULL;
	}
	free(subs);
	subs = NULL;
	return ssd;
}


/****************************************************************************
*this function is to calculate the processing time and the state transition 
*of the processing when processing the write request for the advanced command
*****************************************************************************/
struct ssd_info *compute_serve_time(struct ssd_info *ssd, unsigned int channel, unsigned int chip, struct sub_request **subs, unsigned int subs_count)
{
	unsigned int i = 0;
	struct sub_request * last_sub = NULL;
	int prog_time = 0;

	for (i = 0; i < subs_count; i++)
	{
		subs[i]->current_state = SR_W_TRANSFER;
		if (last_sub == NULL)
		{
			subs[i]->current_time = ssd->current_time;
		}
		else
		{
			subs[i]->current_time = last_sub->complete_time + ssd->parameter->time_characteristics.tDBSY;
		}

		subs[i]->next_state = SR_COMPLETE;
		subs[i]->next_state_predict_time = subs[i]->current_time + 7 * ssd->parameter->time_characteristics.tWC + (subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		subs[i]->complete_time = subs[i]->next_state_predict_time;
		last_sub = subs[i];

		delete_from_channel(ssd, channel, subs[i]);
	}

	ssd->channel_head[channel].current_state = CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time = ssd->current_time;
	ssd->channel_head[channel].next_state = CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time = (last_sub->complete_time>ssd->channel_head[channel].next_state_predict_time) ? last_sub->complete_time : ssd->channel_head[channel].next_state_predict_time;


	prog_time = ssd->parameter->time_characteristics.tPROGO;

	ssd->channel_head[channel].chip_head[chip].current_state = CHIP_WRITE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time = ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
	ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->channel_head[channel].next_state_predict_time + prog_time;

	for (i = 0; i < subs_count; i++)
	{
		subs[i]->next_state_predict_time = subs[i]->next_state_predict_time + prog_time;
		subs[i]->complete_time = subs[i]->next_state_predict_time;
		if (subs[i]->gc_flag == 1)
		{
			free(subs[i]->location);
			subs[i]->location = NULL;
			free(subs[i]);
			subs[i] = NULL;
		}
	}
	return ssd;
}

//compute read time 
struct ssd_info* compute_read_serve_time(struct ssd_info* ssd, unsigned int channel, unsigned int chip, struct sub_request** subs, unsigned int subs_count)
{
	unsigned int i = 0;
	struct sub_request* last_sub = NULL;
	int read_time = 0;

	read_time = ssd->parameter->time_characteristics.tR;

	ssd->channel_head[channel].chip_head[chip].current_state = CHIP_WRITE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time = ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state = CHIP_IDLE;
	ssd->channel_head[channel].chip_head[chip].next_state_predict_time = ssd->current_time + read_time;

	for (i = 0; i < subs_count; i++)
	{
		subs[i]->current_state = SR_R_DATA_TRANSFER;
		if (last_sub == NULL)
		{
			subs[i]->current_time = ssd->channel_head[channel].chip_head[chip].next_state_predict_time;
		}
		else
		{
			subs[i]->current_time = last_sub->complete_time + ssd->parameter->time_characteristics.tDBSY;
		}
		subs[i]->next_state = SR_COMPLETE;
		subs[i]->next_state_predict_time = subs[i]->current_time + 7 * ssd->parameter->time_characteristics.tWC + (subs[i]->size * ssd->parameter->subpage_capacity) * ssd->parameter->time_characteristics.tWC;
		subs[i]->complete_time = subs[i]->next_state_predict_time;
		last_sub = subs[i];

		delete_from_channel(ssd, channel, subs[i]);
	}

	ssd->channel_head[channel].current_state = CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time = ssd->current_time;
	ssd->channel_head[channel].next_state = CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time = (last_sub->complete_time > ssd->channel_head[channel].next_state_predict_time) ? last_sub->complete_time : ssd->channel_head[channel].next_state_predict_time;

	return ssd;
}

/*****************************************************************************************
*Function is to remove the request from ssd-> subs_w_head or ssd-> channel_head [channel] .subs_w_head
******************************************************************************************/
struct ssd_info* delete_from_channel(struct ssd_info* ssd, unsigned int channel, struct sub_request* sub_req)
{
	struct sub_request* sub = NULL, * p, * del_sub;
	struct sub_request* head, * tail;
	unsigned int op_flag;

	op_flag = sub_req->operation;

	if (op_flag == READ)
	{
		head = ssd->channel_head[channel].subs_r_head;
		tail = ssd->channel_head[channel].subs_r_tail;
	}
	else 
	{
		head = ssd->channel_head[channel].subs_w_head;
		tail = ssd->channel_head[channel].subs_w_tail;
	}
		
	sub = head;
	p = sub;
	while (sub != NULL)
	{
		if (sub == sub_req)
		{
			if (sub == head)
			{
				if (head!=tail)
				{
					if (op_flag == READ)
					{
						ssd->channel_head[channel].subs_r_head = sub->next_node;
						sub = ssd->channel_head[channel].subs_r_head;
					}
					else
					{
						ssd->channel_head[channel].subs_w_head = sub->next_node;
						sub = ssd->channel_head[channel].subs_w_head;
					}
					break;
				}
				else
				{
					if (op_flag == READ)
					{
						ssd->channel_head[channel].subs_r_head = NULL;
						ssd->channel_head[channel].subs_r_tail = NULL;
					}
					else
					{
						ssd->channel_head[channel].subs_w_head = NULL;
						ssd->channel_head[channel].subs_w_tail = NULL;
					}
					break;
				}
			}
			else
			{
				if (sub->next_node != NULL) //not tail
				{
					p->next_node = sub->next_node;
					sub = p->next_node;
					break;
				}
				else
				{
					if (op_flag == READ)
					{
						ssd->channel_head[channel].subs_r_tail = p;
						ssd->channel_head[channel].subs_r_tail->next_node = NULL;
					}
					else
					{
						ssd->channel_head[channel].subs_w_tail = p;
						ssd->channel_head[channel].subs_w_tail->next_node = NULL;
					}
					break;
				}
			}
		}
		p = sub;
		sub = sub->next_node;
	}
	return ssd;
}