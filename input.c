#include<fcntl.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>

int main(){
	int fd,r,w,c,i;
	char buff[100];
	printf("o_start¥n");
	fd = open("/dev/tactsw", O_RDONLY);
	if(fd == -1){
		perror("opener：");
		exit(1);
	}
	printf("o_finish¥n");
	printf("r_start¥n");
	for(i=0;i<5;i++){
		r = read(fd,buff,1);
		if(r == -1){
			perror("reader：");
			exit(1);
		}
		printf("r_finish¥n");
		printf("w_start¥n");
		w=write(1,buff,1);
		if(w == -1){
			perror("writeer：");
			exit(1);
		}
	}
	printf("w_finish¥n");
	printf("c_start¥n");
	c=close(fd);
	if(c == -1){
		perror("closeer：");
		exit(1);
	}
	printf("c_finish¥n");
}
