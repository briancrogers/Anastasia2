#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ana_common.h"

#if defined(MEM_TEST)

#define MEMSIZE 20

int main(int argc, char *argv) {
	struct ana_memory my_mem;
	void *mem[MEMSIZE], *temp_ptr;
	int a, b, rnd, size;
	memset(mem, 0, MEMSIZE * sizeof(void *));
	init_ana_memory(&my_mem, NULL, 123456);
	for (b=0; b<1000; b++) {
		if (b % 20==0) {
			temp_ptr = ana_malloc(&my_mem, 141343214, __FILE__, __LINE__);
			if (temp_ptr!=NULL) {
				ana_free(&my_mem, temp_ptr);
			}
		}
		//printf("-------- %d --------\n", b);
		rnd = rand() % MEMSIZE;
		size = rand() % 1024;
		if (mem[rnd]!=NULL) {
			ana_free(&my_mem, mem[rnd]);
			//printf("Freeing mem[%d]\n", rnd);
			check_mem(&my_mem);
		}
		check_mem(&my_mem);
		mem[rnd] = ana_malloc(&my_mem, size, __FILE__, __LINE__);
		check_mem(&my_mem);
		if (mem[rnd]==NULL) {
			rnd = rand() % MEMSIZE;
			if (mem[rnd]!=NULL) {
				ana_free(&my_mem, mem[rnd]);
				//printf("Freeing mem[%d]\n", rnd);
				check_mem(&my_mem);
			}
		} else {
			//printf("Allocating mem[%d] - size %d - address %d - address %d\n", rnd, size, &mem[rnd], (&mem[rnd]+size));
			check_mem(&my_mem);
		}
		//printf("Slots allocated - ");
		for (a=0; a<MEMSIZE; a++) {
			if (mem[a]!=NULL) {
				//printf(" %d,", a);
			}
		}
		//printf(".\n");
		check_mem(&my_mem);
	}
	for (a=0; a<MEMSIZE; a++) {
		if (mem[a]!=NULL) {
			ana_free(&my_mem, mem[a]);
			check_mem(&my_mem);
			//printf("Freeing mem[%d]\n", a);
		}
	}
	printf("Finished with mem_free = %d - mem_used = %d.\n", my_mem.mem_free, my_mem.mem_used);
	printf("Finished");
}
#endif

//Function to initialise our memory structure.. and allocate the memory required
int init_ana_memory(struct ana_memory *my_mem, void *mem_pool, long size) {
    void *mem_ptr;
    ana_hash_item_ptr this_hash_item_ptr;
    //Now lets allocate the memory for our pool of memory
    mem_ptr = apr_palloc(mem_pool, size);
    if (mem_ptr==0) {
        return 0;
    }
    my_mem->memory_start = mem_ptr;
    //Lets allocate some memory for the first mem block
    this_hash_item_ptr = (struct ana_hash_item *) apr_palloc(mem_pool, sizeof(struct ana_hash_item));
    if (this_hash_item_ptr==0) {
#if defined(MEM_TEST)
        free(my_mem->memory_start);
#endif
        return 0;
    }
    memset(this_hash_item_ptr, 0, sizeof(struct ana_hash_item));
    this_hash_item_ptr->size = size;
    this_hash_item_ptr->ptr = mem_ptr;
    this_hash_item_ptr->num = 0;
    strcpy(this_hash_item_ptr->file, __FILE__);
    this_hash_item_ptr->line = __LINE__;
    this_hash_item_ptr->hprevious = 0;
    this_hash_item_ptr->hnext = 0;
    this_hash_item_ptr->lprevious = 0;
    this_hash_item_ptr->lnext = 0;
    this_hash_item_ptr->used=0;
    //Lets clear the size list
    memset(my_mem->size_list, 0, sizeof(ana_hash_item_ptr) * ANASIZEHASH);
    memset(my_mem->used_list, 0, sizeof(ana_hash_item_ptr) * ANAHASHSIZE);
    my_mem->size_list[(ANASIZEHASH - 1)] = this_hash_item_ptr;
    my_mem->mem_free = size;
    my_mem->mem_used = 0;
    my_mem->mem_allocated = size;
    my_mem->number_of_blocks = 1;
    my_mem->number_of_blocks_used = 0;
    my_mem->memory_error = 0;
    my_mem->mem_pool = mem_pool;
    my_mem->this_book = 0;
	my_mem->first_block = this_hash_item_ptr;
/*	tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmpDbgFlag |= _CRTDBG_DELAY_FREE_MEM_DF;
	tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
	tmpDbgFlag |= _CRTDBG_CHECK_ALWAYS_DF;
	_CrtSetDbgFlag(tmpDbgFlag);*/
    return 1;
}

void *ana_malloc(struct ana_memory *my_mem, long size, char *file, int line) {
    ana_hash_item_ptr this_mem_ptr, next_mem_ptr;
    unsigned hindex, a=0;
    int psize, rnd;
    void *new_ptr;
    if (size>=ANASIZEHASH) {
        hindex = (ANASIZEHASH -1);
    } else {
        hindex = size;
    }
    this_mem_ptr = my_mem->size_list[hindex];
    while (this_mem_ptr!=0 && this_mem_ptr->size<size) {
        this_mem_ptr = this_mem_ptr->hnext;
    }
    if (this_mem_ptr==0) {
        //Hmm couldn't allocate a memory block of the right size,
        //so try and find the next largest block
        for (a=(hindex+1); a < ANASIZEHASH && this_mem_ptr==0; a++) {
            this_mem_ptr = my_mem->size_list[a];
        }
		if (a!=0) { a--; }
	if (this_mem_ptr==0) {
		//O.k. big problems here, there was no memory of the right size.
		//First try clearing the hash lilo
//		clear_sgml_items(my_mem->this_book);
		//So try reclaiming all the memory we can
		//Run through all the block until we don't get any consequtive used blocks
/*		cub = 1;
		while (cub==1) {
			cub = 0;
			//Check for consequtive used blocks
			this_mem_ptr = my_mem->first_block;
			while (this_mem_ptr!=NULL && this_mem_ptr->lnext!=NULL) {
				if (this_mem_ptr->used==0 && this_mem_ptr->lnext->used==0) {
					cub = 1;
					ana_reclaim(my_mem, this_mem_ptr);
					this_mem_ptr = my_mem->first_block;
					break;
				}
				this_mem_ptr = this_mem_ptr->lnext;
			}
		}*/
	}
	this_mem_ptr = my_mem->size_list[hindex];
        for (a=(hindex+1); a < ANASIZEHASH && this_mem_ptr==0; a++) {
            this_mem_ptr = my_mem->size_list[a];
        }
	if (a!=0) { a--; }
        if (this_mem_ptr==0) {
            //Big problem
            my_mem->memory_error = 1;
            return 0;
        }
    }
    //Should have found a block
    //Remove this mem block from the hash
    //But *KEEP* the points to the linked list as we can keep these current
    //So when we have to free the memory all we have to do is move the
    //hash object back to the free list and merge with any unused hash objects
    //in the linked list... thats the theory anyway
    if (this_mem_ptr->hprevious!=0) {
        this_mem_ptr->hprevious->hnext = this_mem_ptr->hnext;
    }
    if (this_mem_ptr->hnext!=0) {
        this_mem_ptr->hnext->hprevious = this_mem_ptr->hprevious;
    }
	hindex = this_mem_ptr->size;
	if (hindex >= ANASIZEHASH) {
		hindex = (ANASIZEHASH - 1);
	}
    if (this_mem_ptr==my_mem->size_list[hindex]) {
        my_mem->size_list[hindex] = this_mem_ptr->hnext;
    }
    this_mem_ptr->hprevious = 0;
    this_mem_ptr->hnext = 0;
    new_ptr = this_mem_ptr->ptr;
    //O.k move this block over to the used list no matter what
    psize = this_mem_ptr->size;
    this_mem_ptr->size = size;
    this_mem_ptr->used = 1;
#if defined(MEM_TEST)
	rnd = rand() % 9;
#else
	rnd=0;
#endif
	this_mem_ptr->num = rnd;
    hindex = (unsigned long) new_ptr % ANAHASHSIZE;
    next_mem_ptr = my_mem->used_list[hindex];
    if (next_mem_ptr==0) {
        my_mem->used_list[hindex] = this_mem_ptr;
    } else {
        //Theres already something in this list so add it to the beginning
		my_mem->used_list[hindex] = this_mem_ptr;
		this_mem_ptr->hnext = next_mem_ptr;
		next_mem_ptr->hprevious = this_mem_ptr;
    }
    my_mem->number_of_blocks_used++;
    my_mem->mem_free -= size;
    my_mem->mem_used += size;
    if (psize!=size) {
        //O.k so we have to cut the block down..
        //So just create a new hash item, add into the lilo and put it back into the size list
        next_mem_ptr = (ana_hash_item_ptr) apr_palloc(my_mem->mem_pool, sizeof(struct ana_hash_item));
        if (next_mem_ptr==0) {
            //Big problem
            my_mem->memory_error = 1;
            return 0;
        }
        next_mem_ptr->ptr = (char *) this_mem_ptr->ptr + size;
        next_mem_ptr->size = psize - size;
        next_mem_ptr->lprevious = 0;
        next_mem_ptr->lnext = 0;
        next_mem_ptr->hprevious = 0;
        next_mem_ptr->hnext = 0;
        next_mem_ptr->used = 0;
        strcpy(next_mem_ptr->file, file);
        next_mem_ptr->line = line;
        //Insert into the lilo
        if (this_mem_ptr->lnext!=0) {
			this_mem_ptr->lnext->lprevious = next_mem_ptr;
            next_mem_ptr->lnext = this_mem_ptr->lnext;
			next_mem_ptr->lprevious = this_mem_ptr;
            this_mem_ptr->lnext = next_mem_ptr;
        } else {
            this_mem_ptr->lnext = next_mem_ptr;
            next_mem_ptr->lprevious = this_mem_ptr;
        }
        //Now insert into the size hash
        if (next_mem_ptr->size >= ANASIZEHASH) {
            psize = (ANASIZEHASH-1);
        } else {
            psize = next_mem_ptr->size;
        }
        this_mem_ptr = my_mem->size_list[psize];
        if (this_mem_ptr!=0) {
			this_mem_ptr->hprevious = next_mem_ptr;
			next_mem_ptr->hnext = this_mem_ptr;
		}
		my_mem->size_list[psize] = next_mem_ptr;
        my_mem->number_of_blocks++;
        //All done..
    }
	memset(new_ptr, rnd, size);
    return new_ptr;
}

//O.k. to free memory all we should have to do is, use the memory address and get the hash object out of the used_list
//Put this back into the size_list and then use the lilo to merge and regions that we can..
int ana_free(struct ana_memory *my_mem, void *this_mem) {
    ana_hash_item_ptr this_hash_ptr, next_hash_ptr;
    unsigned int hindex;
    if (my_mem==0 || this_mem==0) {
        return 0;
    }
    hindex = (unsigned long) this_mem % ANAHASHSIZE;
    this_hash_ptr = my_mem->used_list[hindex];
    while (this_hash_ptr!=0 && this_hash_ptr->ptr != this_mem) {
        this_hash_ptr = this_hash_ptr->hnext;
    }
    if (this_hash_ptr==0 || this_hash_ptr->ptr!=this_mem) {
        return 0;
    }
	memset(this_mem, 0, this_hash_ptr->size);
    //O.k. now remove this from the used hash array
    if (this_hash_ptr==my_mem->used_list[hindex]) {
        my_mem->used_list[hindex] = this_hash_ptr->hnext;
		if (this_hash_ptr->hnext!=0) {
			this_hash_ptr->hnext->hprevious = 0;
		}
        this_hash_ptr->hnext = 0;
        this_hash_ptr->hprevious = 0;
    } else {
        if (this_hash_ptr->hprevious!=0) {
            this_hash_ptr->hprevious->hnext = this_hash_ptr->hnext;
        }
        if (this_hash_ptr->hnext!=0) {
            this_hash_ptr->hnext->hprevious = this_hash_ptr->hprevious;
        }
    }
	this_hash_ptr->hprevious = NULL;
	this_hash_ptr->hnext = NULL;
    this_hash_ptr->used = 0;
    //Put it into the size array
    hindex = this_hash_ptr->size;
    if (hindex >= ANASIZEHASH) {
        hindex = (ANASIZEHASH - 1);
    }
    if (my_mem->size_list[hindex]==0) {
        my_mem->size_list[hindex]=this_hash_ptr;
        this_hash_ptr->hprevious = 0;
        this_hash_ptr->hnext = 0;
    } else {
		//Just put it at the start
        next_hash_ptr = my_mem->size_list[hindex];
		if (next_hash_ptr!=0) {
			next_hash_ptr->hprevious = this_hash_ptr;
			this_hash_ptr->hnext = next_hash_ptr;
		}
		my_mem->size_list[hindex] = this_hash_ptr;
    }
	my_mem->number_of_blocks_used--;
	my_mem->mem_free += this_hash_ptr->size;
	my_mem->mem_used -= this_hash_ptr->size;
	ana_reclaim(my_mem, this_hash_ptr);	
    return 1;
}

void *ana_realloc(struct ana_memory *my_mem, void **this_mem, long oldsize, long size, char *file, int line) {
	void *new_ptr;
	//Firstly malloc the required memory
	new_ptr = ana_malloc(my_mem, size, file, line);
	if (new_ptr==0) {
		return 0;
	}
	//Copy over the old memory
	memcpy(new_ptr, *this_mem, oldsize);
	//Now free the old bit of memory
	ana_free(my_mem, *this_mem);
	return new_ptr;
}

int free_ana_memory(struct ana_memory *my_mem) {
	//Frees all the memory associated with the ana_memory structure... 
	//Simple go through the linked list and free all the memory blocks
	ana_hash_item_ptr this_hash_item = my_mem->first_block;
	ana_hash_item_ptr next_hash_item;
	//First free the memory
	if (my_mem->memory_start==0) {
		my_mem->memory_error = 1;
		return 0;
	}
#if defined(MEM_TEST)
	    free(my_mem->memory_start);
#endif
	if (this_hash_item==0) {
		my_mem->memory_error = 1;
		return 0;
	}
	while (this_hash_item!=0) {
		next_hash_item = this_hash_item->lnext;
#if defined(MEM_TEST)
		free(this_hash_item);
#endif
		this_hash_item = next_hash_item;
	}
	my_mem->number_of_blocks = 0;
	my_mem->number_of_blocks_used = 0;
	my_mem->mem_allocated = 0;
	my_mem->mem_free = 0;
	my_mem->mem_pool = 0;
	my_mem->memory_start = 0;
    memset(my_mem->size_list, 0, sizeof(ana_hash_item_ptr) * ANASIZEHASH);
    memset(my_mem->used_list, 0, sizeof(ana_hash_item_ptr) * ANAHASHSIZE);
	my_mem->first_block = 0;
	return 1;
}

void *ana_reclaim(struct ana_memory *my_mem, ana_hash_item_ptr input_hash_ptr) {
	ana_hash_item_ptr this_hash_ptr = input_hash_ptr, next_hash_ptr;
	unsigned int hindex = 0;
    //See if we can merge this block with any others...
	//Check the previous and next hash lilo of this hash pointer and merge when appropriate
	while (this_hash_ptr!=0 && this_hash_ptr->lprevious!=0 && this_hash_ptr->used==0 &&  this_hash_ptr->lprevious->used==0 && ((char *) this_hash_ptr->lprevious->ptr + this_hash_ptr->lprevious->size)==(char *) this_hash_ptr->ptr) {
		//O.k. merge by increase the size of the current block and deleting the previous block
		//Firstly remove this block from the size array
		hindex = this_hash_ptr->size;
		if (hindex >= ANASIZEHASH) {
			hindex = ANASIZEHASH - 1;
		}
		next_hash_ptr = my_mem->size_list[hindex];
		if (next_hash_ptr==this_hash_ptr) {
			my_mem->size_list[hindex] = this_hash_ptr->hnext;
		}
		if (this_hash_ptr->hnext!=0) {
			this_hash_ptr->hnext->hprevious = this_hash_ptr->hprevious;
		}
		if (this_hash_ptr->hprevious!=0) {
			this_hash_ptr->hprevious->hnext = this_hash_ptr->hnext;
		}
		this_hash_ptr->hnext = 0;
		this_hash_ptr->hprevious = 0;
		//Now increase the size of this hash
		this_hash_ptr->size += this_hash_ptr->lprevious->size;
		this_hash_ptr->ptr = this_hash_ptr->lprevious->ptr;
		//And put it into where it should be in the size hash array
		hindex = this_hash_ptr->size;
		if (hindex >= ANASIZEHASH) {
			hindex = ANASIZEHASH - 1;
		}
		next_hash_ptr = my_mem->size_list[hindex];
		if (next_hash_ptr==0) {
			my_mem->size_list[hindex] = this_hash_ptr;
		} else {
			this_hash_ptr->hprevious = 0;
			next_hash_ptr->hprevious = this_hash_ptr;
			this_hash_ptr->hnext = next_hash_ptr;
			my_mem->size_list[hindex] = this_hash_ptr;
		}
		//Done moving this block around
		//So lets get rid of the previous block
		next_hash_ptr = this_hash_ptr->lprevious;
		//Remove the links to the previous block
		this_hash_ptr->lprevious = next_hash_ptr->lprevious;
		if (this_hash_ptr->lprevious!=0) {
			this_hash_ptr->lprevious->lnext = this_hash_ptr;
		} else {
			//If theres no previous then it should be the first in the list
			my_mem->first_block = this_hash_ptr;
		}
		//Remove from the hash array
		hindex = next_hash_ptr->size;
		if (hindex >= ANASIZEHASH) {
			hindex = ANASIZEHASH - 1;
		}
		if (next_hash_ptr->hprevious==0) {
			//Its the first in the size array
			my_mem->size_list[hindex] = next_hash_ptr->hnext;
			if (next_hash_ptr->hnext!=0) {
				next_hash_ptr->hnext->hprevious = 0;
			}
		} else {
			//Just remove the links
			if (next_hash_ptr->hprevious!=0) {
				next_hash_ptr->hprevious->hnext = next_hash_ptr->hnext;
			}
			if (next_hash_ptr->hnext!=0) {
				next_hash_ptr->hnext->hprevious = next_hash_ptr->hprevious;
			}
		}
		my_mem->number_of_blocks--;
		//Check to see if we are freeing the first block
		if (next_hash_ptr==my_mem->first_block) {
			my_mem->first_block = next_hash_ptr->lnext;
		}
#if defined(MEM_TEST)
		free(next_hash_ptr);
#endif
	}
	//Now check the next lilo's
	while (this_hash_ptr!=0 && this_hash_ptr->lnext!=0 && this_hash_ptr->used==0 && this_hash_ptr->lnext->used==0 && ((char *) this_hash_ptr->lnext->ptr)==((char *) this_hash_ptr->ptr + this_hash_ptr->size)) {
		//O.k. merge by increase the size of the current block and deleting the next block
		//Firstly remove this block from the size array
		hindex = this_hash_ptr->size;
		if (hindex >= ANASIZEHASH) {
			hindex = ANASIZEHASH - 1;
		}
		next_hash_ptr = my_mem->size_list[hindex];
		if (next_hash_ptr==this_hash_ptr) {
			my_mem->size_list[hindex] = this_hash_ptr->hnext;
			if (this_hash_ptr->hnext!=0) {
				this_hash_ptr->hnext->hprevious = 0;
			}
		}
		if (this_hash_ptr->hnext!=0) {
			this_hash_ptr->hnext->hprevious = this_hash_ptr->hprevious;
		}
		if (this_hash_ptr->hprevious!=0) {
			this_hash_ptr->hprevious->hnext = this_hash_ptr->hnext;
		}
		this_hash_ptr->hnext = 0;
		this_hash_ptr->hprevious = 0;
		//Now increase the size of this hash
		this_hash_ptr->size += this_hash_ptr->lnext->size;
		//And put it into where it should be in the size hash array
		hindex = this_hash_ptr->size;
		if (hindex >= ANASIZEHASH) {
			hindex = ANASIZEHASH - 1;
		}
		next_hash_ptr = my_mem->size_list[hindex];
		if (next_hash_ptr==0) {
			my_mem->size_list[hindex] = this_hash_ptr;
		} else {
			this_hash_ptr->hprevious = 0;
			next_hash_ptr->hprevious = this_hash_ptr;
			this_hash_ptr->hnext = next_hash_ptr;
			my_mem->size_list[hindex] = this_hash_ptr;
		}
		//Done moving this block around
		//So lets get rid of the next block
		next_hash_ptr = this_hash_ptr->lnext;
		this_hash_ptr->lnext = next_hash_ptr->lnext;
		//Remove the links to the next block
		if (next_hash_ptr->lnext!=0) {
			next_hash_ptr->lnext->lprevious = this_hash_ptr;
		}
		//Remove from the hash array
		hindex = next_hash_ptr->size;
		if (hindex >= ANASIZEHASH) {
			hindex = ANASIZEHASH - 1;
		}
 		if (next_hash_ptr->hprevious==0) {
			//Its the first in the size array
			my_mem->size_list[hindex] = next_hash_ptr->hnext;
			if (next_hash_ptr->hnext!=0) {
				next_hash_ptr->hnext->hprevious = 0;
			}
		} else {
			//Just remove the links
			if (next_hash_ptr->hprevious!=0) {
				next_hash_ptr->hprevious->hnext = next_hash_ptr->hnext;
			}
			if (next_hash_ptr->hnext!=0) {
				next_hash_ptr->hnext->hprevious = next_hash_ptr->hprevious;
			}
		}
		my_mem->number_of_blocks--;
		this_hash_ptr = next_hash_ptr->lnext;
		//Check to see if we are freeing the first block
		if (next_hash_ptr==my_mem->first_block) {
			my_mem->first_block = next_hash_ptr->lnext;
		}
#if defined(MEM_TEST)
		free(next_hash_ptr);
#endif
	}
	return this_hash_ptr;
}

int check_mem(struct ana_memory *my_mem) {
	int free_mem=0, used_mem=0;
	int used_blocks=0, free_blocks=0;
	ana_hash_item_ptr this_hash_item;
	this_hash_item = my_mem->first_block;
	while (this_hash_item!=0) {
		if (this_hash_item->used==0) {
			free_blocks++;
			free_mem += this_hash_item->size;
		} else {
			used_blocks++;
			used_mem += this_hash_item->size;
		}
		this_hash_item = this_hash_item->lnext;
	}
	printf("Checking memory\n");
	printf("free_mem %d (%d) - used_mem - %d (%d) - total_mem %d (%d).\n", free_mem, (int) my_mem->mem_free, used_mem, (int) my_mem->mem_used, (free_mem+used_mem), (int) my_mem->mem_allocated);
	printf("Used blocks %d (%d), Total Blocks %d (%d).\n", (int) my_mem->number_of_blocks_used, used_blocks, (int) my_mem->number_of_blocks, free_blocks+used_blocks);
	if ((free_mem+used_mem)!=my_mem->mem_allocated) {
		printf("Error!!!");
	}
	if (used_blocks!=my_mem->number_of_blocks_used || (free_blocks+used_blocks)!=my_mem->number_of_blocks) {
		printf("Error!!!");
	}
	deep_check(my_mem);
	return 1;
}

int deep_check(struct ana_memory *my_mem) {
	//loop through the block list and count up the used and free memory..
	int mem_used=0, mem_free=0, a;
	int num_blocks=0, num_used_blocks=0;
	ana_hash_item_ptr this_hash_item = my_mem->first_block;
	while (this_hash_item) {
		num_blocks++;
		if (this_hash_item->lprevious==0 && this_hash_item!=my_mem->first_block) {
			printf("Error!!");
		}
		if (this_hash_item->lprevious!=0 && this_hash_item->lprevious->lnext!=this_hash_item) {
			printf("Error!");
		}
		if (this_hash_item->lnext!=0 && this_hash_item->lnext->lprevious!=this_hash_item) {
			printf("Error!");
		}
		if (this_hash_item->used==0) {
			mem_free += this_hash_item->size;
		} else {
			mem_used += this_hash_item->size;
			num_used_blocks++;
		}
		this_hash_item = this_hash_item->lnext;
	}
	if (mem_used!=my_mem->mem_used || mem_free!=my_mem->mem_free || mem_free+mem_used!=my_mem->mem_allocated || num_blocks!=my_mem->number_of_blocks || num_used_blocks!=my_mem->number_of_blocks_used) {
		printf("Error");
	}
	num_blocks = 0;
	//Check the memory array
	for (a=0; a < ANAHASHSIZE; a++) {
		this_hash_item = my_mem->used_list[a];
		if (this_hash_item==0) continue;
		while (this_hash_item!=0) {
			num_blocks++;
			if (this_hash_item == this_hash_item->hnext) {
				printf("Error!");
			}
			if ((unsigned long) this_hash_item->ptr % ANAHASHSIZE!=a) {
				printf("Error!");
			}
			if (this_hash_item->lprevious==0 && this_hash_item!=my_mem->first_block) {
				printf("Error!");
			}
			this_hash_item = this_hash_item->hnext;
		}
	}
	if (num_blocks!=my_mem->number_of_blocks_used) {
		printf("Error!");
	}
	num_blocks = 0;
	//Check the memory array
	for (a=0; a < ANASIZEHASH; a++) {
		this_hash_item = my_mem->size_list[a];
		if (this_hash_item==0) continue;
		if (this_hash_item->hprevious!=0) {
			printf("Error!!");
		}
		while (this_hash_item!=0) {
			num_blocks++;
			if (this_hash_item == this_hash_item->hnext) {
				printf("Error!");
			}
			if (this_hash_item->lprevious==0 && this_hash_item!=my_mem->first_block) {
				printf("Error!");
			}
			if (this_hash_item->size<ANASIZEHASH && this_hash_item->size!=a) {
				printf("Error!");
			}
			if (this_hash_item->hnext!=0 && this_hash_item->size!=this_hash_item->hnext->size && this_hash_item->size<ANASIZEHASH) {
				printf("Error!");
			}
			this_hash_item = this_hash_item->hnext;
		}
	}
	if (num_blocks!=my_mem->number_of_blocks - my_mem->number_of_blocks_used) {
		printf("Error!");
	}
	return 1;
}

