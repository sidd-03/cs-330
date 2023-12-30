#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wait.h>
#include <unistd.h>
#include <stdlib.h>

int child_size(const char* path) {
    DIR *dir;
    struct dirent *entry;
     struct stat file_stat;
    if (stat(path, &file_stat) != 0) {
        return -1;
    }
    	int total_size = file_stat.st_size;
    if ((dir = opendir(path)) == NULL) {
        printf("Unable to execute\n");
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        

        struct stat check;
        if (stat(full_path, &check) != 0) {
            printf("Unable to execute\n");
            continue;
        }
          
        if (S_ISDIR(check.st_mode)) {
          //  printf("%s\n", entry->d_name);
         
        
            int subdir_size = child_size(full_path);
            total_size+= subdir_size;
       
       

        } 
      else if(S_ISLNK(check.st_mode))  total_size+=child_size(full_path);
        else {
           total_size += check.st_size;
          //  printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
    return total_size;
    
}



int main(int argc, char * argv[]){

     if (argc != 2) {
      printf("Unable to execute\n");
        return 1;
    }

    DIR * dir;
    struct dirent * entry;
    char * path = argv[1];

     struct stat file_stat;
    if (lstat(path, &file_stat) != 0) {
        return -1;
    }
    	int total_size = file_stat.st_size;



    if ((dir = opendir(path)) == NULL) {
        printf("Unable to execute\n");
        return -1;
    }
    
     int pipe_fd[2];
        if(pipe(pipe_fd)==-1){
             printf("Unable to execute\n");
               return 1;
                         }

    while((entry = readdir(dir))!=NULL ){

         if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        

        struct stat dir_check;
        if (lstat(full_path, &dir_check) != 0) {
           printf("Unable to execute\n");
            continue;
        }
     
        if(S_ISDIR(dir_check.st_mode)){
            
            int subd_size;
           pid_t child = fork();
           if(child==0) 
          { 
            close(pipe_fd[0]); //closed read end
            subd_size = child_size(full_path);
            write(pipe_fd[1], &subd_size, sizeof(subd_size));
                    close(pipe_fd[1]); 
                    exit(0);
            
           }
        
        else{
            wait(NULL);
            
            read(pipe_fd[0], &subd_size, sizeof(subd_size));

            total_size+=subd_size;


        }
     }
    else if(S_ISLNK(dir_check.st_mode))  total_size+=child_size(full_path);
          
     else{
            total_size+= dir_check.st_size;
    
        }
     


    }
    
    printf("%d\n", total_size);

return 0;

}







