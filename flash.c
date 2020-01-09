#define _CRTDBG_MAP_ALLOC

#include <stdlib.h>
#include <crtdbg.h>

#include "initialize.h"
#include "ssd.h"
#include "flash.h"
#include "buffer.h"
#include "interface.h"
#include "ftl.h"
#include "fcl.h"


/******************************************************************************************
*function is to erase the operation, the channel, chip, die, plane under the block erase
*******************************************************************************************/
Status erase_operation(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block)
{
	unsigned int i = 0;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].plane_erase_count++;

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num = ssd->parameter->page_block;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num = 0;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page = -1;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].erase_count++;

	for (i = 0; i<ssd->parameter->page_block; i++)
	{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state = PG_SUB;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state = 0;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn = -2;
	}
	ssd->erase_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page += ssd->parameter->page_block;

	return SUCCESS;
}

/******************************************************************************************
*function is to read out the old active page, set invalid, migrate to the new valid page
*******************************************************************************************/
Status move_page(struct ssd_info * ssd, struct local *location, unsigned int move_plane,unsigned int * transfer_size)
{
	return SUCCESS;
}

Status  NAND_read(struct ssd_info *ssd, struct sub_request * req)
{
	unsigned int chan, chip, die, plane, block, page;
	chan = req->location->channel;
	chip = req->location->chip;
	die = req->location->die;
	plane = req->location->plane;
	block = req->location->block;
	page = req->location->page;

	if (ssd->channel_head[chan].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].block_type == DATA_BLK)
	{
		ssd->data_read_cnt++;
		if (req->read_flag == UPDATE_READ)
			ssd->data_update_cnt++;
		else
			ssd->data_req_cnt++;
	}
	else
	{
		ssd->tran_read_cnt++;
		if (req->read_flag == UPDATE_READ)
			ssd->tran_update_cnt++;
		else
			ssd->tran_req_cnt++;
	}
	ssd->read_count++;
	return SUCCESS;
}


Status  NAND_program(struct ssd_info *ssd, struct sub_request * req)
{
	unsigned int channel, chip, die, plane, block, page,lpn;

	lpn = req->luns[0];  //page

	channel = req->location->channel;
	chip  = req->location->chip;
	die   = req->location->die;
	plane = req->location->plane;
	block = req->location->block;
	page  = req->location->page;

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page++;  //ilitialization is -1

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_write_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lpn = lpn;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state = 1024;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].free_state = ssd->dram->map->map_entry[lpn].state;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].written_count++;

	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page != page)
		return FAILURE;

	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].block_type == DATA_BLK)
	{
		ssd->data_program_cnt++;
	}
	else
	{
		ssd->tran_program_cnt++;
	}
   
	ssd->write_flash_count++;
	ssd->program_count++;
	return SUCCESS;
}

Status NAND_multi_plane_program(struct ssd_info* ssd, struct sub_request* req0, struct sub_request* req1)
{
	unsigned flag0,flag1;
	flag0 = NAND_program(ssd, req0);
	flag1 = NAND_program(ssd, req1);

	return (flag0 & flag1);
}

Status NAND_multi_plane_read(struct ssd_info* ssd, struct sub_request* req0, struct sub_request* req1)
{
	unsigned flag0, flag1;
	flag0 = NAND_read(ssd, req0);
	flag1 = NAND_read(ssd, req1);

	return (flag0 & flag1);
}

/*********************************************************************************************
*this function is a simulation of a real write operation, to the pre-processing time to use
*********************************************************************************************/
Status write_page(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int active_block, unsigned int *ppn)
{
	int last_write_page = 0;
	last_write_page = ++(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page);
	if (last_write_page >= (int)(ssd->parameter->page_block))
	{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = 0;
		printf("error! the last write page larger than max!!\n");
		return ERROR;
	}

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[last_write_page].written_count++;
	ssd->write_flash_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].test_pre_count++;
	
	*ppn = find_ppn(ssd, channel, chip, die, plane, active_block, last_write_page);
	
	return SUCCESS;
}

/***********************************************************************************************************
*function is to modify the page page to find the state and the corresponding dram in the mapping table value
***********************************************************************************************************/
struct ssd_info *flash_page_state_modify(struct ssd_info *ssd, struct sub_request *sub, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane, unsigned int block, unsigned int page)
{
	unsigned int ppn, full_page;
	struct local *location;
	struct direct_erase *new_direct_erase, *direct_erase_node;

	return ssd;
}

