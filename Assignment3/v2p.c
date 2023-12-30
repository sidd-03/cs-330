#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */
#define MIN_SIZE 4096
 void flush_tlb(){
            u64 cr3_value;
    asm volatile(
        "mov %%cr3, %0"
        : "=r"(cr3_value));
    asm volatile(
        "mov %0, %%rax\n\t"
        "mov %%rax, %%cr3"
        :
        : "r"(cr3_value)
        : "eax");

       // printk("tlb flushed\n");
        return;
               }               
struct vm_area *create_new_vma(u64 start, int length, int prot) {
    struct vm_area *vma = (struct vm_area *)os_alloc(sizeof(struct vm_area));
    if(vma == NULL) return NULL;
    vma->vm_start = start;
    vma->vm_end = start + length;
    vma->access_flags = prot;
    vma->vm_next = NULL;
    return vma;
}
long find_lowest(struct vm_area* head, int length, int prot){
    struct vm_area * curr = head->vm_next;
    struct vm_area * prev = head;
    u64 res;
    while(curr){
        if(prev->vm_end + length <= curr->vm_start ){
          res = prev->vm_end;
         
        break;
         //return prev->vm_end;
        }
        prev = curr;
        curr = curr->vm_next;

    }
    int merged1 = 0;
    int merged2 = 0;
    if(curr==NULL) res = prev->vm_end; //if not enough free space in between
    if(prev->access_flags == prot) {
        merged1 = 1;
        prev->vm_end+=length;
        }
    if(curr!=NULL && curr->access_flags==prot && res+length==curr->vm_start){
        if(merged1){
        prev->vm_end = curr->vm_end;
        prev->vm_next = curr->vm_next;
        
        os_free(curr, sizeof(struct vm_area));
        stats->num_vm_area--;
        }
        else{
            curr->vm_start = res;

        }
        merged2=1;
    }
    if(!merged1 && !merged2){
         struct vm_area * new = create_new_vma(res,length,prot); //if not merging
         new->vm_next = curr;
         prev->vm_next = new;
         if(new!= NULL){
            stats->num_vm_area++;
            return new->vm_start;

         } 
         else return 0;
    }
   
   return res;
}
struct vm_area * find_addr(u64 addr, struct vm_area * list){
     struct vm_area* prev_vma = NULL;
     struct vm_area *current_vma = list;
     int mapped = 0;
    
        while (current_vma) {
            if (addr >= current_vma->vm_start && addr < current_vma->vm_end) {
                mapped = 1;
                break; // address is already mapped
            }
            prev_vma = current_vma;
          
            current_vma = current_vma->vm_next;
        }
        if(mapped) return prev_vma; //return prev vmarea
        else return NULL;

}

void merge(struct vm_area * list){
    struct vm_area* curr = list->vm_next;
    struct vm_area* prev = list;

     while(curr){
    if((prev->access_flags == curr->access_flags) && (prev->vm_end==curr->vm_start)){
     //   printk("merged\n");
        prev->vm_end = curr->vm_end;
        prev->vm_next=curr->vm_next;
        os_free(curr,sizeof(struct vm_area));
         stats->num_vm_area--;
       curr = prev->vm_next;
    }

    else {
        prev = curr;
        curr = curr->vm_next;
    }
   }

}

int page_free(struct exec_context * current, u64 addr){
    u64 pgd_offset = (addr & 0x0000FF8000000000)>>39;
    u64 pud_offset = (addr & 0x0000007FC0000000)>>30;
    u64 pmd_offset = (addr & 0x000000003FE00000)>>21;
    u64 pte_offset = (addr & 0x00000000001FF000)>>12;

    u64 * pgd_entry = osmap(current->pgd) + pgd_offset;
    u64 pgd_val = *(pgd_entry);
     if(!(pgd_val & 0x1)) return 0;

    u64 * pud = osmap(pgd_val  >> 12);
    u64 * pud_entry = pud + pud_offset;
    u64 pud_val = *(pud_entry);
     if(!(pud_val & 0x1)) return 0;

    u64 * pmd = osmap(pud_val >> 12);
    u64 * pmd_entry = pmd + pmd_offset;
    u64 pmd_val = *(pmd_entry);
     if(!(pmd_val & 0x1)) return 0;

    u64 * pte = osmap(pmd_val >> 12);
    u64 * pte_entry = pte + pte_offset;
    u64 pte_val = *(pte_entry);
     if(!(pte_val & 0x1)) return 0;

    u64 pfn_to_free = pte_val>>12;
    if(get_pfn_refcount(pfn_to_free)==1) {
       // printk("page free\n");
        put_pfn(pfn_to_free);
        os_pfn_free(USER_REG, pfn_to_free);
      
    

    }
    else{
         put_pfn(pfn_to_free);

    }
    *(pte_entry) = 0;
 //  printk("page free entry %x\n", *(pte_entry));

    return 0;


}
int page_protect(struct exec_context * current, u64 addr,int prot){
    u64 pgd_offset = (addr & 0x0000FF8000000000)>>39;
    u64 pud_offset = (addr & 0x0000007FC0000000)>>30;
    u64 pmd_offset = (addr & 0x000000003FE00000)>>21;
    u64 pte_offset = (addr & 0x00000000001FF000)>>12;

    u64 * pgd_entry = osmap(current->pgd) + pgd_offset;
    u64 pgd_val = *(pgd_entry);
     if(!(pgd_val & 0x1)) return 0;

    u64 * pud = osmap((pgd_val & 0xffffffffffffffff)>> 12);
    u64 * pud_entry = pud + pud_offset;
    u64 pud_val = *(pud_entry);
     if(!(pud_val & 0x1)) return 0;

    u64 * pmd = osmap((pud_val& 0xffffffffffffffff) >> 12);
    u64 * pmd_entry = pmd + pmd_offset;
    u64 pmd_val = *(pmd_entry);
     if(!(pmd_val & 0x1)) return 0;

    u64 * pte = osmap((pmd_val& 0xffffffffffffffff) >> 12);
    u64 * pte_entry = pte + pte_offset;
    u64 pte_val = *(pte_entry);
     if(!(pte_val & 0x1)) return 0;
      
    if((get_pfn_refcount((pte_val& 0xffffffffffffffff)>>12)) > 1) return 0;

    if(prot == PROT_WRITE || prot == (PROT_WRITE| PROT_READ)){
        pgd_val |=0x8;
        pud_val |=0x8;
        pmd_val |=0x8;
        pte_val |=0x8;
       //  printk("1. changed to write\n");

       }
    if(prot == PROT_READ){
        pte_val &=0xFFFFFFFFFFFFFFF7;
      //  printk("changed to read only\n");
       }
      
       *(pte_entry) = pte_val;
       *(pgd_entry) = pgd_val;
       *(pud_entry) = pud_val;
       *(pmd_entry) = pmd_val;

    //    printk("pte_val: %x\n",*(pte_entry));
    //    printk("pgd_val: %x\n",*(pgd_entry));
    //    printk("pmd_val: %x\n",*(pmd_entry));
    //    printk("pud_val: %x\n",*(pud_entry));
    return 0;


}

void copy_entry(u64 addr,u32 parent_pgd,u32 child_pgd){
    //  printk("copy entry addr %x\n", addr);   

    u64 pgd_offset = (addr>>39) & 0x1FF;
    u64 pud_offset = (addr>>30) & 0x1FF;
    u64 pmd_offset = (addr>> 21)& 0x1FF;
    u64 pte_offset = (addr>>12) & 0x1FF;

    u64 * pgd_entry = (u64*)osmap(parent_pgd) + pgd_offset;
    u64 * cpgd_entry = (u64*)osmap(child_pgd) + pgd_offset;
    u64 pgd_val = *(pgd_entry);
    u64 cpgd_val= *(cpgd_entry);
    // printk("pg. pgdval %x\n",pgd_val);
    if(!(pgd_val & 1)){
        return ;
        }
        else{
            if(!(cpgd_val & 1)){
                u64 new_pfn = os_pfn_alloc(OS_PT_REG);
                cpgd_val = ((new_pfn << 12) | (pgd_val & 0xFFF));
              *(cpgd_entry) = cpgd_val;
      // printk("pg. new_pg child pgdval %x\n",cpgd_val);
        //page alloc
            }
       
    }
    //  printk("pg. pgdval2 %x\n",pgd_val);
    //   printk("pg. pudoffset %x\n",pud_offset);
    //   printk("pud page address %x\n",osmap(pgd_val>>12));
    u64 * pud_entry = (u64*)osmap((pgd_val)>>12) + pud_offset;

    // printk("pg. pudoffset %x\n",pud_entry);

    u64 * cpud_entry = (u64*)osmap(cpgd_val>>12) + pud_offset;
    u64 pud_val = *(pud_entry);
    u64 cpud_val = *(cpud_entry);
    //  printk("pg. pudval %x\n",pud_val);
    //  printk("pg. child pudval %x\n",*(cpud_entry));
    if(!(pud_val & 1)){
        return ;
        }
        else{
       if(!(cpud_val & 1)){
                u64 new_pfn = os_pfn_alloc(OS_PT_REG);
                cpud_val = ((new_pfn << 12) | (pud_val & 0xFFF));
                 *(cpud_entry) = cpud_val;
     //  printk("pg. new_pg child pudval %x\n",*(cpud_entry));
        //page alloc
            }
    }
    u64 * pmd_entry = (u64*)osmap(pud_val>>12) + pmd_offset;
    u64 * cpmd_entry = (u64*)osmap(cpud_val>>12) + pmd_offset;
    u64 pmd_val = *(pmd_entry);
    u64 cpmd_val = *(cpmd_entry);
    // printk("pg. pmdval %x\n",pmd_val);
    if(!(pmd_val & 1)){
        return ;
        }
        else{
       if(!(cpmd_val & 1)){
                u64 new_pfn = os_pfn_alloc(OS_PT_REG);
                cpmd_val = ((new_pfn << 12)|(pmd_val & 0xFFF));
              *(cpmd_entry) = cpmd_val;
    //   printk("pg. new_pg child pmdval %x\n",cpmd_val);
        //page alloc
            }
    }

    u64 * pte_entry =(u64*) osmap(pmd_val>>12) + pte_offset;
    u64 * cpte_entry =(u64*) osmap(cpmd_val>>12) + pte_offset;
    u64 pte_val = *(pte_entry);
    u64 cpte_val = *(cpte_entry);
    // printk("pg. pteval %x\n",pte_val);
    if(!(pte_val & 1)){
        return ;
        }
        else{
       if(!(cpte_val & 1)){
               
                cpte_val = (pte_val & (~(1<<3)));
                pte_val = (pte_val & (~(1<<3)));
              *(pte_entry) = pte_val;
              *(cpte_entry) = cpte_val;
              get_pfn(cpte_val>>12);
    //   printk("pg. new_pg pteval %x\n",cpte_val);
        //page alloc
            }
    }
   return ;
    
}

void copy_segments(u64 start,u64 end,u32 parent_pgd,u32 child_pgd){
    while(start<end)
    {
        copy_entry(start,parent_pgd,child_pgd);
        start+=4096;

    }

}

void copy_vm_area(struct exec_context *parent, struct exec_context *child){
    struct vm_area * curr = parent->vm_area;
    struct vm_area * new = create_new_vma(curr->vm_start,curr->vm_end-curr->vm_start,curr->access_flags);
    child->vm_area = new;
   // struct vm_area * prev = new; 
   // new=new->vm_next;
    curr=curr->vm_next;
    while(curr){
         new->vm_next = create_new_vma(curr->vm_start,curr->vm_end-curr->vm_start,curr->access_flags);
         new= new->vm_next;
         u64 start= curr->vm_start;
         u64 end = curr->vm_end;
         while(start<end){
            copy_entry(start,parent->pgd,child->pgd);
            start+=4096;
         }
        // prev->vm_next = new;
        // prev = new;
         curr = curr->vm_next;
    }

}
struct vm_area * check_left(struct vm_area * node,struct vm_area * list){

    struct vm_area * curr = list->vm_next;
    struct vm_area * prev = list;
    while(curr){
        if(prev->vm_end <= node->vm_start && curr->vm_start>= node->vm_end){
            
          break;
        }
        prev = curr;
        curr = curr->vm_next;
       }
      return prev;

}

/**
 * mprotect System call Implementation.
 */
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    // printk("inside mprotect\n");
    if(prot<=0 || length<0) return -EINVAL;
    length = ((length + 4095) / 4096 )* 4096;
    u64 end_addr = addr + length;
   
    struct vm_area * prev = current->vm_area;
    struct vm_area * curr = current->vm_area->vm_next;
    
    while(curr){
     //   printk("inside while 1\n");
        if(addr >= curr->vm_start && addr < curr->vm_end)
        {
            if(addr == curr->vm_start && end_addr>=curr->vm_end ){
                curr->access_flags = prot;
             //   printk("1. changed prot\n");
             
                
            }
            else if(addr > curr->vm_start && end_addr>=curr->vm_end){
                 struct vm_area* split = create_new_vma(addr,curr->vm_end-addr,prot);
                 if(!split) {
                    return -1;
                 }
                curr->vm_end = addr;  
                split->vm_next = curr->vm_next;
                curr->vm_next = split;
                curr  = split; //
                stats->num_vm_area++;

            }
            else if(addr == curr->vm_start && end_addr<curr->vm_end){
                struct vm_area* split = create_new_vma(end_addr,curr->vm_end-end_addr,curr->access_flags);
                if(!split) {
                    return -1;
                 }
                curr->vm_end = end_addr;
                curr->access_flags  = prot;
                split->vm_next = curr->vm_next;
                curr->vm_next = split;
                stats->num_vm_area++;
                 merge(current->vm_area);
              //   printk("here\n");
                while(addr != end_addr){
               page_protect(current,addr,prot);
               addr+=4096;
                  }
                  flush_tlb();
               //    printk("called flush\n");
               
                return 0;
                
            }
            else if(addr > curr->vm_start && end_addr<curr->vm_end){
                
               struct vm_area* split = create_new_vma(addr,end_addr-addr,prot);
               if(!split) {
                    return -1;
                 }
               struct vm_area* split2 = create_new_vma(end_addr,curr->vm_end - end_addr,curr->access_flags);
               if(!split2) {
                    return -1;
                 }
               curr->vm_end = addr;
               split2->vm_next = curr->vm_next;
               split->vm_next = split2;
               curr->vm_next = split;
               stats->num_vm_area+=2;

                merge(current->vm_area);
               while(addr != end_addr){
              page_protect(current,addr,prot);
              addr+=4096;
             }
             
               flush_tlb();
         //   printk("called flush\n");
               return 0;
                 

            }
        }
        else if(addr < curr->vm_start  && end_addr>=curr->vm_end){
            curr->access_flags=prot;
        }
        else if(addr < curr->vm_start  && end_addr<curr->vm_end){
            struct vm_area* split = create_new_vma(end_addr,curr->vm_end-end_addr,curr->access_flags);
            if(!split) {
                    return -1;
                 }
                curr->vm_end = end_addr;
                curr->access_flags  = prot;
                split->vm_next = curr->vm_next;
                curr->vm_next = split;
                stats->num_vm_area++;
                
                 merge(current->vm_area);
                while(addr != end_addr){
              page_protect(current,addr,prot);
               addr+=4096;
                }
                flush_tlb();
           //       printk("called flush\n");
                return 0;
                

        }
        curr=curr->vm_next;

    }

     merge(current->vm_area);
   while(addr != end_addr){
           page_protect(current,addr,prot);
            addr+=4096;
        }

flush_tlb();
 // printk("called flush\n");
return 0;


    
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
    //check flags and prot
     if(prot!=PROT_READ && prot!=(PROT_READ|PROT_WRITE))return -1;

     if(flags!=0 && flags!=MAP_FIXED) return -1;

   if (length <= 0 || (addr!= 0 && (addr < MMAP_AREA_START || addr + length > MMAP_AREA_END)) || length >=MMAP_AREA_END-MMAP_AREA_START) {
        return  -EINVAL;
    }
  
    if(length>0x200000) return -1;
     u64 res ;
    if(current->vm_area == NULL) { //dummy
        current->vm_area = create_new_vma(MMAP_AREA_START,4096,0);
        if(current->vm_area == NULL) return -EINVAL;
         stats->num_vm_area++;
      }
   length = ((length + 4095) / 4096 )* 4096;
 
    if(addr == 0){
        if(flags==MAP_FIXED) return -1;

        else{
            
         res  = find_lowest(current->vm_area,length,prot);   
         
         return res; 
        }
    }
    else{ //not null
        if (length <= 0 || (addr!= 0 && (addr < MMAP_AREA_START || addr + length > MMAP_AREA_END))) {
        return  -EINVAL;
    }

   
    struct vm_area *current_vma = current->vm_area;
    int mapped = 0;
    int merged1 = 0;
    int merged2 = 0;
        while (current_vma) {
            if ((addr >= current_vma->vm_start && addr < current_vma->vm_end)) {
                
                mapped = 1;
                break; // address is already mapped
            }
            current_vma = current_vma->vm_next;
        }
        current_vma = current->vm_area; //check if end address is mapped
        while (current_vma) {
            if ((addr+length > current_vma->vm_start && addr+length <= current_vma->vm_end)) {
                
                mapped = 1;
                break; // address is already mapped
            }
            current_vma = current_vma->vm_next;
        }
        current_vma = current->vm_area; 
        while (current_vma) {
            if ((addr <= current_vma->vm_start && addr+length >= current_vma->vm_end)) {
                
                mapped = 1;
                break; // address is already mapped
            }
            current_vma = current_vma->vm_next;
        }
         if(mapped == 0){

         struct vm_area * new = create_new_vma(addr,length,prot);
         if(!new) {
                    return -1;
                 }
         struct vm_area * left = check_left(new , current->vm_area);
         struct vm_area * right = left->vm_next;
          if(left->vm_end == new->vm_start && left->access_flags==new->access_flags){ //merge leftside
                left->vm_end = new->vm_end;
                merged1=1;
                // os_free(node,sizeof(struct vm_area));
                
            }
          if(right!=NULL && right->access_flags==prot && new->vm_end == right->vm_start){ //merge right side
                if(merged1){
                   left->vm_end = right->vm_end;    
                   left->vm_next = right->vm_next;
                   
                }
                else{
                    new->vm_end = right->vm_end;
                    new->vm_next = right->vm_next;
                }
              merged2 = 1;
            
               os_free(right, sizeof(struct vm_area));
               stats->num_vm_area--;
              }
            if(merged1) {
                os_free((new),sizeof(struct vm_area));
            }
            if(!merged1 && !merged2){
                 new->vm_next = left->vm_next;
                left->vm_next = new; //add in list
                stats->num_vm_area++;
               
            }

            return addr;
        

        }
         else if(flags == MAP_FIXED && mapped==1) return -1;
  
         else if(flags == 0 && mapped==1){
          res  = find_lowest(current->vm_area,length,prot);
          return res;   

   }
          else return -1;

    }
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    if(addr <=0 || length<0 )
    return -EINVAL;
     length = ((length + 4095) / 4096 )* 4096;
     u64 end_addr = addr + length;
   
    struct vm_area * to_free;
    struct vm_area * prev = current->vm_area;
    struct vm_area * curr = current->vm_area->vm_next;

    int count = 0;
   
        while (curr) {
            count++;
            if (addr >= curr->vm_start && addr < curr->vm_end) {
                if(addr == curr->vm_start && end_addr>=curr->vm_end){
                 //   printk("unmap loop 1\n");
                to_free = curr;
                prev->vm_next = curr->vm_next;
                curr = prev;
                os_free(to_free,sizeof(struct vm_area));
                stats->num_vm_area--;
              //  printk("unmap %d\n",stats->num_vm_area);
            }
            else if(addr > curr->vm_start && end_addr>=curr->vm_end){
               //  printk("unmap loop 2\n");
                curr->vm_end = addr;
            }
            else if(addr == curr->vm_start && end_addr<curr->vm_end){
               //  printk("unmap loop 3\n");
                curr->vm_start = end_addr;
            }
            else if(addr > curr->vm_start && end_addr<curr->vm_end){
              //   printk("unmap loop 4\n");
                struct vm_area* split = create_new_vma(end_addr,curr->vm_end-end_addr,curr->access_flags);
                curr->vm_end = addr;
                split->vm_next = curr->vm_next;
                curr->vm_next = split;
                stats->num_vm_area++;
             //   printk("unmap %d\n",stats->num_vm_area);
                 

            }
            
            }
          else  if(addr<curr->vm_start  && end_addr>=curr->vm_end){
         //   printk("unmap loop 5\n");
                 to_free = curr;
                 prev->vm_next = curr->vm_next;
                 curr = prev;
                 os_free(to_free,sizeof(struct vm_area));
                 stats->num_vm_area--;
            //     printk("unmap %d\n",stats->num_vm_area);
            }
            else if(addr<curr->vm_start && (end_addr>curr->vm_start && end_addr<curr->vm_end)){
             //    printk("unmap loop 6\n");
                curr->vm_start = end_addr;

            }
            prev = curr;
            curr = curr->vm_next;
        }

        while(addr != end_addr){
            page_free(current,addr);
            addr+=4096;
        }
        flush_tlb();
        return 0;
    

}



/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

 long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code) {   
    // printk("pg. error code %x\n",error_code);
    //  printk("pg. in vm_area_pagefault\n");
    struct  vm_area * curr = current->vm_area;
    int mapped = 0;
    while(curr){
        if(addr >= curr->vm_start && addr < curr->vm_end){
           // printk("pg. vm_area found\n");

            mapped = 1;
            break;
        }
        curr = curr->vm_next;
    
    }
    int valid_access = 0;
    if(!mapped) return -1;
    if(error_code == 0x4)  valid_access = 1;
    else if(error_code==0x6){
       //  printk("pg. valid write access\n");
        if(curr->access_flags == PROT_WRITE || curr->access_flags == (PROT_WRITE|PROT_READ)){
             valid_access = 1;
        }
        else return -1;
    
    }
    else if(error_code == 0x7){
        if(curr->access_flags == PROT_WRITE || curr->access_flags == (PROT_WRITE|PROT_READ)){
            return handle_cow_fault(current,addr,curr->access_flags);
        }
        else return -1;
    }
    // else{
    //      return -1;
    // }
    if(!valid_access) return -1;

    u64 pgd_offset = (addr>>39) & 0x1FF;
    u64 pud_offset = (addr>>30) & 0x1FF;
    u64 pmd_offset = (addr>> 21)& 0x1FF;
    u64 pte_offset = (addr>>12) & 0x1FF;

    u64 * pgd_entry = osmap(current->pgd) + pgd_offset;
    u64 pgd_val = *(pgd_entry);
    // printk("pg. pgdval %x\n",pgd_val);
    if(!(pgd_val & 0x1)){
       u64 new_pfn = os_pfn_alloc(OS_PT_REG);
       pgd_val = (new_pfn << 12);
       pgd_val |= 0x1;
       if(curr->access_flags == PROT_WRITE || curr->access_flags ==(PROT_WRITE|PROT_READ)){
        pgd_val |=0x8;

       }
       pgd_val |=0x10;
       *(pgd_entry) = pgd_val;
     //  printk("pg. new_pg pgdval %x\n",pgd_val);
        //page alloc
    }
    u64 * pud = osmap(pgd_val  >> 12);
    u64 * pud_entry = pud + pud_offset;
    u64 pud_val = *(pud_entry);
   // printk("pg. pudval %x\n",pud_val);
    if(!(pud_val & 0x1)){
         u64 new_pfn = os_pfn_alloc(OS_PT_REG);
       pud_val = (new_pfn << 12);
       pud_val |= 0x1;
       if(curr->access_flags == PROT_WRITE || curr->access_flags == (PROT_WRITE|PROT_READ)){
        pud_val |=0x8;

       }
       pud_val |=0x10;
       *(pud_entry) = pud_val;
    //   printk("pg. new_pg pudval %x\n",pud_val);

    }
    u64 * pmd = osmap(pud_val >> 12);
    u64 * pmd_entry = pmd + pmd_offset;
    u64 pmd_val = *(pmd_entry);
    // printk("pg. pmdval %x\n",pmd_val);
    if(!(pmd_val & 0x1)){
     u64 new_pfn = os_pfn_alloc(OS_PT_REG);
       pmd_val = (new_pfn << 12);
       pmd_val |= 0x1;
       if(curr->access_flags == PROT_WRITE || curr->access_flags == (PROT_WRITE|PROT_READ)){
        pmd_val |=0x8;

       }
       pmd_val |=0x10;
       *(pmd_entry) = pmd_val;
     //   printk("pg. new_pg pmdval %x\n",pmd_val);

    }

    u64 * pte = osmap(pmd_val >> 12);
    u64 * pte_entry = pte + pte_offset;
    u64 pte_val = *(pte_entry);

    // printk("pg. pteval %x\n",pte_val);
    if(!(pte_val & 0x1)){
       u64 new_pfn = os_pfn_alloc(USER_REG);
       pte_val = (new_pfn << 12);
       pte_val |= 0x1;
       if(curr->access_flags == PROT_WRITE || curr->access_flags == (PROT_WRITE|PROT_READ)){
        pte_val |=0x8;

       }
       pte_val |=0x10;
       *(pte_entry) = pte_val;
   // printk("pg. new_pg pteval %x\n",pte_val);

    }
    
    
    return 1;
}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
     pid = new_ctx->pid;
    memcpy(new_ctx,ctx,sizeof(struct exec_context));
    
    new_ctx->pid = pid;
    new_ctx->ppid = ctx->pid;

    
    
   //FILE ARRAY COPY??
    u64 child_pgd = os_pfn_alloc(OS_PT_REG);
    new_ctx->pgd = child_pgd;
    
    copy_segments(ctx->mms[MM_SEG_CODE].start, ctx->mms[MM_SEG_CODE].next_free, ctx->pgd,child_pgd);
    copy_segments(ctx->mms[MM_SEG_RODATA].start, ctx->mms[MM_SEG_RODATA].next_free, ctx->pgd,child_pgd);
    copy_segments(ctx->mms[MM_SEG_DATA].start, ctx->mms[MM_SEG_DATA].next_free, ctx->pgd,child_pgd);
    copy_segments(ctx->mms[MM_SEG_STACK].start, ctx->mms[MM_SEG_STACK].end, ctx->pgd,child_pgd);
    copy_vm_area(ctx,new_ctx);


     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{
  //  printk("cow fault va: %x\n",vaddr);
    if(access_flags==PROT_READ) return -1;


    u64 pgd_offset = (vaddr & 0x0000FF8000000000)>>39;
    u64 pud_offset = (vaddr & 0x0000007FC0000000)>>30;
    u64 pmd_offset = (vaddr & 0x000000003FE00000)>>21;
    u64 pte_offset = (vaddr & 0x00000000001FF000)>>12;

    u64 * pgd_entry = osmap(current->pgd) + pgd_offset;
    u64 pgd_val = *(pgd_entry);
    

    u64 * pud = osmap(pgd_val  >> 12);
    u64 * pud_entry = pud + pud_offset;
    u64 pud_val = *(pud_entry);
   

    u64 * pmd = osmap(pud_val >> 12);
    u64 * pmd_entry = pmd + pmd_offset;
    u64 pmd_val = *(pmd_entry);
     

    u64 * pte = osmap(pmd_val >> 12);
    u64 * pte_entry = pte + pte_offset;
    u64 pte_val = *(pte_entry);

    u32 source_pfn = (pte_val& 0xffffffffffffffff)>>12;

   if(get_pfn_refcount(source_pfn)==1) {
     pte_val|=0x8;
      *(pte_entry) = pte_val;
      flush_tlb();
      return 1;
    }
    u64 new_pfn = os_pfn_alloc(USER_REG);
    memcpy(osmap(new_pfn),osmap(source_pfn),4096);
   
  if(get_pfn_refcount(source_pfn)>1)  put_pfn(source_pfn);
  else{
 //   printk("ref count is less than zero %d",get_pfn_refcount(source_pfn));
  }
    u64 new_pte_val = ((new_pfn<<12)| (pte_val & 0x0000000000000FFF));
     new_pte_val |=0x8;
    *(pte_entry) = new_pte_val;
    flush_tlb();

    return 1;

  

  
  
}
