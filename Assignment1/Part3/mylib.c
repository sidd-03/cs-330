#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MEGABYTES_4 (4 * 1024 * 1024) // 4 MB
#define min_chunk 24
void * free_head = NULL;


void* memalloc(unsigned long size) {
    printf("memalloc() called\n");
    if (size == 0) {
        return NULL;
    }
     unsigned long chunk_size = size + sizeof(unsigned long);

    
  
    if (chunk_size % 8 != 0) {
        chunk_size += 8 - (chunk_size % 8);
    }
   
    if (chunk_size < min_chunk) {
        chunk_size = min_chunk;
    }


    void* check = free_head;
    void* prev = NULL;

    while (check != NULL) {
        unsigned long check_size = *((unsigned long*)check);

        if (check_size >= chunk_size) {
            
            if (prev != NULL) {
                *((void**)((char*)prev + 8)) = *((void**)((char*)check + 8));
            } 
            else {
                free_head = *((void**)((char*)check + 8));
            }

            
            if (check_size > chunk_size) {
                unsigned long remaining_size = check_size - chunk_size;
                void* remaining_chunk = (void*)((char*)check + chunk_size);

                *((unsigned long*)remaining_chunk) = remaining_size;
                *((void**)((char*)remaining_chunk + 8)) = free_head;
                free_head = remaining_chunk;
            }

           
            *((unsigned long*)check) = chunk_size;

        //  print(check);
            
            return (char*)check + sizeof(unsigned long); 
        }

        prev = check;
        check = *((void**)((char*)check + 8));
    }

    unsigned long mmap_size = ((chunk_size / MEGABYTES_4) + 1) * MEGABYTES_4;

    

 
    void* allocated_chunk = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocated_chunk == MAP_FAILED) {
        printf("Unable to execute\n");
        return NULL;
    }

   
    *((unsigned long*)allocated_chunk) = chunk_size;

  
    void* leftover = (void*)((char*)allocated_chunk + chunk_size);
    *((unsigned long*)leftover) = mmap_size - chunk_size;
    *((void**)((char*)leftover + 8)) = free_head;
    free_head = leftover;

   
    return (char*)allocated_chunk + sizeof(unsigned long); 
}
int memfree(void* ptr) {
    printf("memfree() called\n");
    if (ptr == NULL) {
        return -1; 
    }

    unsigned long* size_ptr = (unsigned long*)ptr - 1; 

    unsigned long allocated_size = *size_ptr;

    void* freed_block = (char*)ptr - sizeof(unsigned long);

    *((void**)(freed_block + sizeof(unsigned long))) = free_head; 
    *((void**)(freed_block + 2 * sizeof(unsigned long))) = NULL;
   
    void* next_block = (char*)freed_block + allocated_size;
    void* prev_block = NULL;
    void* current_block = free_head;

    while (current_block != NULL) {
       
        if (current_block == next_block) {
           
            allocated_size += *((unsigned long*)next_block);

            
            *((void**)(freed_block + sizeof(unsigned long))) = *((void**)(next_block + sizeof(unsigned long)));

            break;
        }

        prev_block = current_block;
        current_block = *((void**)(current_block + sizeof(unsigned long)));
    }

  
    if (prev_block != NULL) {
       
        unsigned long left_size = *((unsigned long*)prev_block);


        if ((char*)prev_block + left_size == freed_block) {
            
            allocated_size += left_size; 

           
            *((void**)(prev_block + sizeof(unsigned long))) = *((void**)(freed_block + sizeof(unsigned long)));

           
            freed_block = prev_block;
        }
    }
    *((unsigned long*)freed_block) = allocated_size;
    *((void**)(freed_block + sizeof(unsigned long))) = free_head;
    free_head = freed_block;

    return 0; 
}
