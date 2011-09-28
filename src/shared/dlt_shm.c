/*
* Dlt- Diagnostic Log and Trace user library
* @licence app begin@
 *
 * Copyright (C) 2011, BMW AG - Alexander Wenzel <alexander.wenzel@bmw.de>
 * 
 * This program is free software; you can redistribute it and/or modify it under the terms of the 
 * GNU Lesser General Public License, version 2.1, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even 
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General 
 * Public License, version 2.1, for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License, version 2.1, along 
 * with this program; if not, see <http://www.gnu.org/licenses/lgpl-2.1.html>.
 * 
 * Note that the copyright holders assume that the GNU Lesser General Public License, version 2.1, may 
 * also be applicable to programs even in cases in which the program is not a library in the technical sense.
 * 
 * Linking DLT statically or dynamically with other modules is making a combined work based on DLT. You may 
 * license such other modules under the GNU Lesser General Public License, version 2.1. If you do not want to 
 * license your linked modules under the GNU Lesser General Public License, version 2.1, you 
 * may use the program under the following exception.
 * 
 * As a special exception, the copyright holders of DLT give you permission to combine DLT 
 * with software programs or libraries that are released under any license unless such a combination is not
 * permitted by the license of such a software program or library. You may copy and distribute such a 
 * system following the terms of the GNU Lesser General Public License, version 2.1, including this
 * special exception, for DLT and the licenses of the other code concerned.
 * 
 * Note that people who make modified versions of DLT are not obligated to grant this special exception 
 * for their modified versions; it is their choice whether to do so. The GNU Lesser General Public License, 
 * version 2.1, gives permission to release a modified version without this exception; this exception 
 * also makes it possible to release a modified version which carries forward this exception.
 *
 * @licence end@
*/


/*******************************************************************************
**                                                                            **
**  SRC-MODULE: dlt_shm.c                                                     **
**                                                                            **
**  TARGET    : linux                                                         **
**                                                                            **
**  PROJECT   : DLT                                                           **
**                                                                            **
**  AUTHOR    : Alexander Wenzel Alexander.AW.Wenzel@bmw.de                   **
**                                                                            **
**  PURPOSE   :                                                               **
**                                                                            **
**  REMARKS   :                                                               **
**                                                                            **
**  PLATFORM DEPENDANT [yes/no]: yes                                          **
**                                                                            **
**  TO BE CHANGED BY USER [yes/no]: no                                        **
**                                                                            **
*******************************************************************************/

/*******************************************************************************
**                      Author Identity                                       **
********************************************************************************
**                                                                            **
** Initials     Name                       Company                            **
** --------     -------------------------  ---------------------------------- **
**  aw          Alexander Wenzel           BMW                                **
*******************************************************************************/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#include <dlt_shm.h>

void dlt_shm_print_hex(char *ptr,int size)
{
    int num;

    for (num=0;num<size;num++)
    {
		if((num%16)==15)
			printf("%.2x\n",((unsigned char*)ptr)[num]);
		else
			printf("%.2x ",((unsigned char*)ptr)[num]);
    }
    printf("\n");
}

void dlt_shm_pv(int id,int operation)
{
	static struct sembuf semaphor;
	
	semaphor.sem_op = operation;
	semaphor.sem_flg = SEM_UNDO;
	
	if(semop(id, &semaphor,1) == -1) {
		perror("semop");
	}
}

int dlt_shm_init_server(DltShm *buf,int key,int size) {
	struct shmid_ds shm_buf;

	// Init parameters
	buf->shm = NULL;
	buf->shmid = 0;
	buf->semid = 0;
	buf->size = 0;
	buf->mem = 0;

    // Create the segment.
    if ((buf->shmid = shmget(key, size, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        return -1; /* ERROR */
    }

	// get the size of shm
	if (shmctl(buf->shmid,  IPC_STAT, &shm_buf))
	{
		perror("shmctl");
        return -1; /* ERROR */
	}	

    // Now we attach the segment to our data space.
    if ((buf->shm = shmat(buf->shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        return -1; /* ERROR */
    }
	
	// Init semaphore
	if( (buf->semid = semget(DLT_SHM_SEM,1,S_IRWXU|S_IRWXG|IPC_CREAT|IPC_EXCL)) == -1 ) {
		if( (buf->semid = semget(DLT_SHM_SEM,1,S_IRWXU|S_IRWXG|IPC_EXCL)) == -1 ) {
			perror("semget");
			return -1; /* ERROR */
		}
	}
	if( semctl(buf->semid,0,SETVAL,(int)1) == -1 ) {
        perror("semctl");
        return -1; /* ERROR */
	}
	
	// Init pointers
    ((int*)(buf->shm))[0] = 0;  // pointer to write memory  
    ((int*)(buf->shm))[1] = 0;  // pointer to read memory
    ((int*)(buf->shm))[2] = 0;  // number of packets
    buf->mem = (char*)(&(((int*)(buf->shm))[3]));
    buf->size = shm_buf.shm_segsz - (buf->mem - buf->shm);

	// clear memory
	memset(buf->mem,0,buf->size);
    
	//dlt_shm_status(buf);
    //dlt_shm_info(buf);
    printf("SHM: Size %d\n",buf->size);    

	return 0; /* OK */
}

int dlt_shm_init_client(DltShm *buf,int key) {
	struct shmid_ds shm_buf;

	// init parameters
	buf->shm = NULL;
	buf->shmid = 0;
	buf->semid = 0;
	buf->size = 0;
	buf->mem = 0;

    // Create the segment.
    if ((buf->shmid = shmget(key, 0, 0666)) < 0) {
        perror("shmget");
        return -1; /* ERROR */
    }

	// get the size of shm
	if (shmctl(buf->shmid,  IPC_STAT, &shm_buf))
	{
		perror("shmctl");
        return -1; /* ERROR */
	}	

    // Now we attach the segment to our data space.
    if ((buf->shm = shmat(buf->shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        return -1; /* ERROR */
    }
        	
	// Init semaphore
	if( (buf->semid = semget(DLT_SHM_SEM,0,0)) == -1 ) {
        perror("semget");
        return -1; /* ERROR */
	}
	DLT_SHM_SEM_FREE(buf->semid);

	// Init pointers
    buf->mem = (char*)(&(((int*)(buf->shm))[3]));
    buf->size = shm_buf.shm_segsz - (buf->mem - buf->shm);
    
	//dlt_shm_status(buf);
    //dlt_shm_info(buf);

	return 0; /* OK */
}

void dlt_shm_info(DltShm *buf)
{

    printf("SHM id: %d\n",buf->shmid);    
    printf("Available size: %d\n",buf->size);    
    printf("SHM full start address: %lX\n",(unsigned long)buf->shm);        
    printf("SHM start address: %lX\n",(unsigned long)buf->mem);        

}

void dlt_shm_status(DltShm *buf)
{
	int write, read, count;

	write = ((int*)(buf->shm))[0];
	read = ((int*)(buf->shm))[1];
	count = ((int*)(buf->shm))[2];

    printf("Write: %d\n",write);    
    printf("Read: %d\n",read);    
    printf("Count: %d\n",count);    

}

int dlt_shm_push(DltShm *buf,const unsigned char *data1,unsigned int size1,const unsigned char *data2,unsigned int size2,const unsigned char *data3,unsigned int size3)
{
	int write, read, count;
	
	if(!buf->mem) {
		// shm not initialised
		printf("SHM not initialised\n");
		return -1; /* ERROR */
	}

	// get semaphore
	DLT_SHM_SEM_GET(buf->semid);

	// get current write pointer
	write = ((int*)(buf->shm))[0];
	read = ((int*)(buf->shm))[1];
	count = ((int*)(buf->shm))[2];
	
	// check space and write pointer
	if(read==write && count) {
		// shm buffer is full
		DLT_SHM_SEM_FREE(buf->semid);
		//printf("SHM is totally full\n");
		return -1; // ERROR
	}
	else if(write >= buf->size) {
		if((size1+size2+size3+sizeof(unsigned char)+sizeof(int)) > read) {
			DLT_SHM_SEM_FREE(buf->semid);
			//printf("SHM is full at start\n");
			return -1; // ERROR
		}	
		write = 0;
	}	
	else if(read > write) {
		if((write + size1+size2+size3+sizeof(unsigned char)+sizeof(int)) > read) {
			DLT_SHM_SEM_FREE(buf->semid);
			//printf("SHM is full at end\n");
			return -1; // ERROR
		}	
	}
	else // read <= write
	{
		if((write+size1+size2+size3+sizeof(unsigned char)+sizeof(int)) > buf->size) {
			// data does not fit at end of buffer
			// try write at beginning
			if((size1+size2+size3+sizeof(unsigned char)+sizeof(int)) > read) {
				DLT_SHM_SEM_FREE(buf->semid);
				//printf("SHM is full at start\n");
				return -1; // ERROR
			}	
			// write zero status and size at end if possible
			if((write+sizeof(unsigned char)+sizeof(int)) <= buf->size) {
				*((unsigned char*)(buf->mem+write)) = 0;  // init write status to unused
				*((int*)(buf->mem+write+sizeof(unsigned char))) = 0;  // init write size to unused
			} 
			
			write = 0;
		}
	}	
	
	// update global shm pointers
	((int*)(buf->shm))[0] = write+sizeof(unsigned char)+sizeof(int)+size1+size2+size3; // set new write pointer 	
	((int*)(buf->shm))[2] += 1; // increase counter

	// update buffer pointers
	*((unsigned char*)(buf->mem+write)) = 1;  // set write status
	*((int*)(buf->mem+write+sizeof(unsigned char))) = size1+size2+size3;  // set write size
	
	// free semaphore
	DLT_SHM_SEM_FREE(buf->semid);

	// write data
	if(data1)
		memcpy(buf->mem+write+sizeof(unsigned char)+sizeof(int),data1,size1);
	if(data2)
		memcpy(buf->mem+write+sizeof(unsigned char)+sizeof(int)+size1,data2,size2);
	if(data3)
		memcpy(buf->mem+write+sizeof(unsigned char)+sizeof(int)+size1+size2,data3,size3);
	
	// update write status
	*((unsigned char*)(buf->mem+write)) = 2;

	return 0; // OK
}

int dlt_shm_pull(DltShm *buf,unsigned char *data, int max_size)
{
	int write, read, count, size;
	unsigned char status;
	
	if(!buf->mem) {
		// shm not initialised
		printf("SHM not initialised\n");
		return -1; /* ERROR */
	}

	// get current write pointer
	DLT_SHM_SEM_GET(buf->semid);
	write = ((int*)(buf->shm))[0];
	read = ((int*)(buf->shm))[1];
	count = ((int*)(buf->shm))[2];
	DLT_SHM_SEM_FREE(buf->semid);

	// check if data is in there
	if(count<=0) {
		//printf("SHM is empty\n");
		return -1; // ERROR		
	}

	// check if end of buffer is reached and read status and size
	if((read+sizeof(unsigned char)+sizeof(int)) <= buf->size) {
		status =  *((unsigned char*)(buf->mem+read));
		size = *((int*)(buf->mem+read+sizeof(unsigned char)));
		if(status == 0) {
			// data fits not end of shm
			read = 0;
			status =  *((unsigned char*)(buf->mem+read));
			size = *((int*)(buf->mem+read+sizeof(unsigned char)));
		}
	}
	else {
		read = 0;
		status =  *((unsigned char*)(buf->mem+read));
		size = *((int*)(buf->mem+read+sizeof(unsigned char)));
	}
	
	// check status
	if(status != 2 ) {
		//printf("Buffer is not fully written\n");
		return -1; // ERROR		
	}
	
	// plausibility check of buffer size
	if( (read+size) > buf->size) {
		printf("Buffers size bigger than shm buffer\n");
		return -1; // ERROR		
	}
	
	// check max read size
	if(size > max_size) {
		printf("Buffer is bigger than max size\n");
		return -1; // ERROR			
	}
	
	// copy data
	memcpy(data,buf->mem+read+sizeof(unsigned char)+sizeof(int),size);

	// update buffer pointers
	((int*)(buf->shm))[1] = read+sizeof(unsigned char)+sizeof(int)+size; // set new read pointer 	
	((int*)(buf->shm))[2] -= 1; // decrease counter

	return size; // OK
}

int dlt_shm_copy(DltShm *buf,unsigned char *data, int max_size)
{
	int write, read, count, size;
	unsigned char status;
	
	if(!buf->mem) {
		// shm not initialised
		printf("SHM not initialised\n");
		return -1; /* ERROR */
	}

	// get current write pointer
	DLT_SHM_SEM_GET(buf->semid);
	write = ((int*)(buf->shm))[0];
	read = ((int*)(buf->shm))[1];
	count = ((int*)(buf->shm))[2];
	DLT_SHM_SEM_FREE(buf->semid);

	// check if data is in there
	if(count<=0) {
		//printf("SHM is empty\n");
		return -1; // ERROR		
	}

	// check if end of buffer is reached and read status and size
	if((read+sizeof(unsigned char)+sizeof(int)) <= buf->size) {
		status =  *((unsigned char*)(buf->mem+read));
		size = *((int*)(buf->mem+read+sizeof(unsigned char)));
		if(status == 0) {
			// data fits not end of shm
			read = 0;
			status =  *((unsigned char*)(buf->mem+read));
			size = *((int*)(buf->mem+read+sizeof(unsigned char)));
		}
	}
	else {
		read = 0;
		status =  *((unsigned char*)(buf->mem+read));
		size = *((int*)(buf->mem+read+sizeof(unsigned char)));
	}
	
	// check status
	if(status != 2 ) {
		//printf("Buffer is not fully written\n");
		return -1; // ERROR		
	}
	
	// plausibility check of buffer size
	if( (read+size) > buf->size) {
		printf("Buffers size bigger than shm buffer\n");
		return -1; // ERROR		
	}
	
	// check max read size
	if(size > max_size) {
		printf("Buffer is bigger than max size\n");
		return -1; // ERROR			
	}
	
	// copy data
	memcpy(data,buf->mem+read+sizeof(unsigned char)+sizeof(int),size);

	return size; // OK
}

int dlt_shm_remove(DltShm *buf)
{
	int write, read, count, size;
	unsigned char status;
	
	if(!buf->mem) {
		// shm not initialised
		printf("SHM not initialised\n");
		return -1; /* ERROR */
	}

	// get current write pointer
	DLT_SHM_SEM_GET(buf->semid);
	write = ((int*)(buf->shm))[0];
	read = ((int*)(buf->shm))[1];
	count = ((int*)(buf->shm))[2];
	DLT_SHM_SEM_FREE(buf->semid);

	// check if data is in there
	if(count<=0) {
		//printf("SHM is empty\n");
		return -1; // ERROR		
	}

	// check if end of buffer is reached and read status and size
	if((read+sizeof(unsigned char)+sizeof(int)) <= buf->size) {
		status =  *((unsigned char*)(buf->mem+read));
		size = *((int*)(buf->mem+read+sizeof(unsigned char)));
		if(status == 0) {
			// data fits not end of shm
			read = 0;
			status =  *((unsigned char*)(buf->mem+read));
			size = *((int*)(buf->mem+read+sizeof(unsigned char)));
		}
	}
	else {
		read = 0;
		status =  *((unsigned char*)(buf->mem+read));
		size = *((int*)(buf->mem+read+sizeof(unsigned char)));
	}
	
	// check status
	if(status != 2 ) {
		//printf("Buffer is not fully written\n");
		return -1; // ERROR		
	}
	
	// plausibility check of buffer size
	if( (read+size) > buf->size) {
		printf("Buffers size bigger than shm buffer\n");
		return -1; // ERROR		
	}
		
	// update buffer pointers
	((int*)(buf->shm))[1] = read+sizeof(unsigned char)+sizeof(int)+size; // set new read pointer 	
	((int*)(buf->shm))[2] -= 1; // decrease counter

	return size; // OK
}

int dlt_shm_free_server(DltShm *buf) {

	if(!buf->shm) {
        printf("Shared memory segment not attached\n");
        return -1; /* ERROR */
    }
		
	if(shmdt(buf->shm)) {
        perror("shmdt");
        return -1; /* ERROR */
    }

	if(shmctl(buf->shmid,IPC_RMID,NULL) == -1) {
        perror("shmdt");
        return -1; /* ERROR */
	}

	if(semctl(buf->semid,0,IPC_RMID,(int)0) == -1) {
        perror("shmdt");
        return -1; /* ERROR */
	}

	// Reset parameters
	buf->shm = NULL;
	buf->shmid = 0;
	buf->semid = 0;
	buf->size = 0;
	buf->mem = 0;
		
	return 0; /* OK */
}

int dlt_shm_free_client(DltShm *buf) {

	if(!buf->shm) {
        printf("Shared memory segment not attached\n");
        return -1; /* ERROR */
    }
		
	if(shmdt(buf->shm)) {
        perror("shmdt");
        return -1; /* ERROR */
    }

	// Reset parameters
	buf->shm = NULL;
	buf->shmid = 0;
	buf->semid = 0;
	buf->size = 0;
	buf->mem = 0;
		
	return 0; /* OK */
}