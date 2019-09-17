#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "fs.h"

void print_error(char* e){
    fprintf(stderr,"%s\n", e);
    exit(1);
}
int main(int argc, char* argv[]){
    if (argc == 1)
        print_error("Usage: xcheck <file_system_image>");
    int fsfd;
    fsfd = open(argv[1], O_RDONLY);
    if (fsfd < 0)
        print_error("image not found.");
    struct stat fs_stat;
    fstat(fsfd, &fs_stat);
    void* fs_ptr = mmap(NULL, fs_stat.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
    struct superblock *sb = (struct superblock*)(fs_ptr+BSIZE);
    struct dinode *curr = (struct dinode*)(fs_ptr+2*BSIZE);
    struct dinode *first_ind = curr;
    int bmap[sb->size];
    int counter = 0;
    int num_blk = (sb->ninodes *sizeof(struct dinode))/BSIZE + 1;
    char* bmap_ptr = fs_ptr + (2*BSIZE) + (num_blk*BSIZE);
    for (int i = 0; i < sb->size; i++){
        int div = 1 << counter;
        bmap[i] = div & *bmap_ptr;
        if (counter >= 7){
            counter = 0;
            bmap_ptr++;
            continue;
        }
        counter++;
    }
    unsigned int blk_stat[sb->size];
    unsigned int blk_used[sb->size];
    unsigned int ind_used[sb->ninodes];
    unsigned int ind_ref[sb->ninodes];
    unsigned int ind_count[sb->ninodes];
    unsigned int dir_count[sb->ninodes];
    for (int i = 0; i<sb->size; i++){
        blk_stat[i] = 0;
        blk_used[i] = 0;
    }
    for (int i = 0; i < sb->ninodes; i++){
        ind_used[i] = 0;
        ind_ref[i] = 0;
        ind_count[i] = 0;
        dir_count[i] = 0;
    }
    for (int i = 0; i < sb->ninodes; i++){
        if (curr->type != 3&& curr->type !=2 && curr->type != 1 && curr->type != 0) print_error("ERROR: bad inode.");
        if(curr->type != 0){
            ind_used[i] = 1;
            for (int j = 0; j < NDIRECT+1; j++){
                if (curr->addrs[j] < 0 ||curr->addrs[j] > 1023) print_error("ERROR: bad direct address in inode.");
                if (bmap[curr->addrs[j]] <= 0) print_error("ERROR: address used by inode but marked free in bitmap.");
                blk_stat[curr->addrs[j]] = 1;
                if (blk_used[curr->addrs[j]] == 1 && curr->addrs[j] != 0) print_error("ERROR: direct address used more than once.");
                blk_used[curr->addrs[j]] = 1;
            }
            unsigned int* blk_ptr =(unsigned int*)(fs_ptr + (BSIZE*(curr->addrs[NDIRECT])));
            for(int j = 0; j < NINDIRECT; j++){
                if (*blk_ptr < 0 || *blk_ptr > 1023)
                    print_error("ERROR: bad indirect address in inode.");
                blk_stat[*blk_ptr] = 1;
                if (blk_used[*blk_ptr] == 1&& *blk_ptr != 0) print_error("ERROR: indirect address used more than once.");
                if (bmap[*blk_ptr] <= 0) print_error("ERROR: address used by inode but marked free in bitmap.");
                blk_used[*blk_ptr] = 1;
                blk_ptr++;
            }
            if (i == ROOTINO){
                if (curr->type != 1)
                    print_error("ERROR: root directory does not exist.");
                struct dirent* dir = fs_ptr + (BSIZE*curr->addrs[0]);
                if (dir->inum == 1){
                    if ((dir+1)->inum != 1)
                        print_error("ERROR: root directory does not exist.");
                }
            }
            if (curr->type == 1){
                struct dirent* first_dir = fs_ptr+(BSIZE*curr->addrs[0]);
                if (strcmp(first_dir->name, ".")) print_error("ERROR: directory not properly formatted.");
                else if (strcmp((first_dir+1)->name, "..")) print_error("ERROR: directory not properly formatted."); 
                struct dirent* dir_curr;
                for (int j = 0; j < NDIRECT; j++) {
                    dir_curr = fs_ptr + (BSIZE*curr->addrs[j]);
                    for (int k = 0; k < (BSIZE/sizeof(struct dirent)); k++){
                        if (dir_curr->inum != 0){
                            ind_ref[dir_curr->inum] = 1;
                            struct dinode *n = (struct dinode*)(first_ind + dir_curr->inum);
                            if (n->type == 2)
                                ind_count[dir_curr->inum]++;
                            if (n->type == 1)
                                if (strcmp(dir_curr->name, ".") != 0
                                        && strcmp(dir_curr->name, "..") != 0)
                                    dir_count[dir_curr->inum]++;   
                        }
                        dir_curr++;
                    }
                }     
                blk_ptr =(unsigned int*)(fs_ptr + (BSIZE*(curr->addrs[NDIRECT])));
                for (int j = 0; j < NINDIRECT; j++) {
                    dir_curr = fs_ptr + ((*blk_ptr)*BSIZE);
                    for (int k = 0; k < (BSIZE/sizeof(struct dirent)); k++){
                        if (dir_curr->inum != 0){
                            ind_ref[dir_curr->inum] = 1;
                            struct dinode *n = (struct dinode*)(first_ind + dir_curr->inum);
                            if (n->type == 2)
                                ind_count[dir_curr->inum]++;
                            if (n->type == 1)
                                if (strcmp(dir_curr->name, ".") != 0
                                        && strcmp(dir_curr->name, "..") != 0)
                                    dir_count[dir_curr->inum]++;   
                        }
                        dir_curr++;
                    }
                    blk_ptr++;
                }
            }
        }
        curr++;
    }
    for (int i = 0; i < sb->size; i++){
        if (i > num_blk+2) if (bmap[i] > 0 && blk_stat[i] == 0) print_error("ERROR: bitmap marks block in use but it is not in use.");
    }
        curr = first_ind;
    for (int i = 0; i < sb->ninodes; i++){
        if (ind_used[i] == 1&&ind_ref[i] == 0)
            print_error("ERROR: inode marked use but not found in a directory.");
        if (ind_used[i] == 0&&ind_ref[i] == 1)
            print_error("ERROR: inode referred to in directory but marked free.");
        if (curr->type == 2&&ind_count[i]!=curr->nlink)
            print_error("ERROR: bad reference count for file.");
        if (curr->type == 1
                && dir_count[i] >= 2)
            print_error("ERROR: directory appears more than once in file system.");
        curr++;
    }
    exit(0);
}