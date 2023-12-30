#include <stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<math.h>
int main(int argc, char* argv[]){

       if(argc<2 || argv[argc - 1]<0)  {
              printf("Unable to execute\n");
              return 1;
       }
       
       unsigned long x = atoi(argv[argc - 1]);
       float a = sqrt(x);
       x = round(a);
       sprintf(argv[argc - 1], "%lu", x);
     
       if(argc<3) {
              printf("%lu\n",x);
              exit(0);
       }
       
       
      else{
               execv(argv[1],argv+1);
              printf("Unable to execute\n");
              return 1;
       
      }
      
             
       
       return 0;

}