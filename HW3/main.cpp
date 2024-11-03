#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include "ext2fs.h"
#include "ext2fs_print.h"
#include "identifier.h"
#include <vector>
#include <string.h>
#include <cmath>

// SEEK_SET bastan, SEEK_CUR simdiki, SEEK_END sondan

using namespace std;

int image;
int block_size;
ext2_super_block super_block;
vector<ext2_block_group_descriptor> bgd_t;
ext2_inode root;

void print_recursive(ext2_inode* inode, int depth);

// ext2_inode get_inode(int inode_num){
//     ext2_inode inode;
//     lseek(image, bgd_t[(inode_num)/super_block.inodes_per_group].inode_table * block_size + ((inode_num-1)%super_block.inodes_per_group) * super_block.inode_size, SEEK_SET);
//     read(image, &inode, sizeof(inode));
//     return inode;
// }

ext2_inode get_inode(int inode_num) {
    ext2_inode inode;

    int block_group = (inode_num - 1) / super_block.inodes_per_group;
    int index = (inode_num - 1) % super_block.inodes_per_group;
    
    off_t offset = bgd_t[block_group].inode_table * block_size + index * super_block.inode_size;
    
    if (lseek(image, offset, SEEK_SET) == (off_t) -1) {
        std::cerr << "Error seeking to inode: " << strerror(errno) << std::endl;
    }
    
    if (read(image, &inode, sizeof(inode)) != sizeof(inode)) {
        std::cerr << "Error reading inode: " << strerror(errno) << std::endl;
    }
    
    return inode;
}

void print_block(int block_index, int depth){
    ext2_dir_entry dir;
    lseek(image, block_index * block_size, SEEK_SET);
    read(image, &dir, sizeof(dir));
    vector<char> name(dir.name_length+1);
    read(image, &(name[0]), dir.name_length);
    name[dir.name_length] = '\0';
    int cur_offset = 0;
    while(dir.inode != 0){ // dir.length yazinca bozuluyor
        //print_dir_entry(&dir, &(name[0]));
        if(strcmp(&(name[0]), ".") == 0  || strcmp(&(name[0]), "..") == 0){

        }
        else{
            for(int j = 0; j < depth; j++){
                cout << "-";
            }

            if(dir.file_type & EXT2_D_DTYPE){
                cout << " " << &(name[0]) << "/" << endl;
                ext2_inode temp_inode = get_inode(dir.inode);
                //cout << "Recursive call for " << &(name[0]) << endl;
                print_recursive(&temp_inode, depth+1);
            }
            else if(dir.file_type & EXT2_D_FTYPE){
                cout << " " << &(name[0])  << endl;
            }
            else{
                cout << "EERRIRIRI" << endl;
            }
        }
        cur_offset += dir.length;
        if(cur_offset >= block_size){
            break;
        }
        lseek(image, block_index * block_size + cur_offset, SEEK_SET);
        read(image, &dir, sizeof(dir));
        name.resize(dir.name_length+1);
        read(image, &(name[0]), dir.name_length);
        name[dir.name_length] = '\0';
    }
    if(dir.inode == 0) {
        return;
    }
    //cout << "Finished this directory" << endl;
}

void print_recursive(ext2_inode* inode, int depth){
    for(int i = 0; i < EXT2_NUM_DIRECT_BLOCKS; i++){
        if(inode->direct_blocks[i] != 0){
            //cout << ""; // Bu nasil hata annamadim
            print_block(inode->direct_blocks[i], depth);
        }
        else{
            //cout << "Block " << i << " is empty" << endl;
        }
    }
    
    if(inode->single_indirect != 0){
        for(int i=0; i < block_size/4; i++){
            uint32_t block_index;
            lseek(image, (inode->single_indirect * block_size) + i*4, SEEK_SET);
            read(image, &block_index, sizeof(block_index));
            //cout << block_index << endl;
            if(block_index != 0) {
                //cout << "GOT  INSIDE and index is " << block_index << endl;
                print_block(block_index, depth);
            }
        }
    }

    if(inode->double_indirect != 0){
        for(int i=0; i < block_size/4; i++){
            uint32_t block_index;
            lseek(image, (inode->double_indirect * block_size) + i*4, SEEK_SET);
            read(image, &block_index, sizeof(block_index));
            if(block_index != 0){
                for(int j=0; j < block_size/4; j++){
                    uint32_t block_index2;
                    lseek(image, block_index * block_size + j*4, SEEK_SET);
                    read(image, &block_index2, sizeof(block_index2));
                    if(block_index2 != 0){
                        print_block(block_index2, depth);
                    }
                }
            }
        }
    }

    if(inode->triple_indirect != 0){
        for(int i=0; i < block_size/4; i++){
            uint32_t block_index;
            lseek(image, (inode->triple_indirect * block_size) + i*4, SEEK_SET);
            read(image, &block_index, sizeof(block_index));
            if(block_index != 0){
                for(int j=0; j < block_size/4; j++){
                    uint32_t block_index2;
                    lseek(image, block_index * block_size + j*4, SEEK_SET);
                    read(image, &block_index2, sizeof(block_index2));
                    if(block_index2 != 0){
                        for(int k=0; k < block_size/4; k++){
                            uint32_t block_index3;
                            lseek(image, block_index2 * block_size + k*4, SEEK_SET);
                            read(image, &block_index3, sizeof(block_index3));
                            if(block_index3 != 0){
                                print_block(block_index3, depth);
                            }
                        }
                    }
                }
            }
        }
    }
    
    return;
}

void print_tree(){
    cout << "- root/" << endl;
    print_recursive(&root, 2);
}

vector<vector<char>> block_bitmap_arr;

void recover_inode_bitmap() {
    for(int i=0; i<bgd_t.size(); i++){
        ext2_block_group_descriptor bgd = bgd_t[i];
        //print_group_descriptor(&bgd);
        char inode_bitmap[super_block.inodes_per_group/8];
        lseek(image, bgd.inode_bitmap * block_size, SEEK_SET);
        read(image, inode_bitmap, sizeof(inode_bitmap));
        for(int j=0; j<sizeof(inode_bitmap); j++){
            for(int k=0; k<8; k++){
                ext2_inode cur_inode = get_inode(i*super_block.inodes_per_group + j*8 + (k) + 1);
                if((i == 0 && j == 0) ||  (i == 0 && j == 1 && k < 3))  inode_bitmap[j] |= (1<<k);
                
                if((inode_bitmap[j] & (1<<k)) == 0){
                    //print_inode(&cur_inode, i*super_block.inodes_per_group + j*8 + (k) + 1);
                    if(cur_inode.mode != 0 && cur_inode.deletion_time == 0 && cur_inode.link_count != 0){
                        //cout << "changed inode at group: " << i << ", index: " << j*8 + (k) << endl;
                        
                        inode_bitmap[j] |= (1<<k);
                    }
                }
            }
        }
        lseek(image, bgd.inode_bitmap * block_size, SEEK_SET);
        if (write(image, inode_bitmap, sizeof(inode_bitmap)) != sizeof(inode_bitmap)) {
            perror("write");
            // handle error
        }
    }
}

void set_block_bitmap(int block_index){
    if(block_bitmap_arr[(block_index)/super_block.blocks_per_group][(block_index%super_block.blocks_per_group)/8] & (1<<(block_index%8))){
        return;
    }
    if(block_size <= 1024){
        
        block_index--;
        //cout << "Block group: " << (block_index)/super_block.blocks_per_group << " relative index: " << (block_index%super_block.blocks_per_group) << " edited" << endl;
        block_bitmap_arr[(block_index)/super_block.blocks_per_group][(block_index%super_block.blocks_per_group)/8] |= (1<<(block_index%8));  
    } 
    else{
        //cout << "Block group: " << (block_index)/super_block.blocks_per_group << " relative index: " << (block_index%super_block.blocks_per_group) << " edited" << endl;
        block_bitmap_arr[(block_index)/super_block.blocks_per_group][(block_index%super_block.blocks_per_group)/8] |= (1<<(block_index%8));
    } 
    
}


void traverse_inodes(){
    for(int i=0; i<bgd_t.size(); i++){
        ext2_block_group_descriptor bgd = bgd_t[i];
        //print_group_descriptor(&bgd);
        for(int j=0; j<super_block.inodes_per_group; j++){
            ext2_inode cur_inode = get_inode(i*super_block.inodes_per_group + j + 1);
            if(cur_inode.mode != 0 && cur_inode.deletion_time == 0 && cur_inode.link_count != 0){
                
                for(int k = 0; k < EXT2_NUM_DIRECT_BLOCKS; k++){
                    if(cur_inode.direct_blocks[k] != 0){
                        set_block_bitmap(cur_inode.direct_blocks[k]);
                    }
                }
                if(cur_inode.single_indirect != 0){
                    for(int k=0; k < block_size/4; k++){
                        uint32_t block_index;
                        lseek(image, (cur_inode.single_indirect * block_size) + k*4, SEEK_SET);
                        read(image, &block_index, sizeof(block_index));
                        if(block_index != 0) {
                            set_block_bitmap(block_index);
                        }
                    }
                }
                if(cur_inode.double_indirect != 0){
                    for(int k=0; k < block_size/4; k++){
                        uint32_t block_index;
                        lseek(image, (cur_inode.double_indirect * block_size) + k*4, SEEK_SET);
                        read(image, &block_index, sizeof(block_index));
                        if(block_index != 0){
                            for(int l=0; l < block_size/4; l++){
                                uint32_t block_index2;
                                lseek(image, block_index * block_size + l*4, SEEK_SET);
                                read(image, &block_index2, sizeof(block_index2));
                                if(block_index2 != 0){
                                    set_block_bitmap(block_index2);
                                }
                            }
                        }
                    }
                }
                if(cur_inode.triple_indirect != 0){
                    for(int k=0; k < block_size/4; k++){
                        uint32_t block_index;
                        lseek(image, (cur_inode.triple_indirect * block_size) + k*4, SEEK_SET);
                        read(image, &block_index, sizeof(block_index));
                        if(block_index != 0){
                            for(int l=0; l < block_size/4; l++){
                                uint32_t block_index2;
                                lseek(image, block_index * block_size + l*4, SEEK_SET);
                                read(image, &block_index2, sizeof(block_index2));
                                if(block_index2 != 0){
                                    for(int m=0; m < block_size/4; m++){
                                        uint32_t block_index3;
                                        lseek(image, block_index2 * block_size + m*4, SEEK_SET);
                                        read(image, &block_index3, sizeof(block_index3));
                                        if(block_index3 != 0){
                                            set_block_bitmap(block_index3);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void recover_block_bitmap() {
    //char block_bitmap_arr[bgdt.size()][super_block.blocks_per_group/8];
    block_bitmap_arr.resize(bgd_t.size(), vector<char>(super_block.blocks_per_group/8));    
    for(int i=0; i<bgd_t.size(); i++){
        for(int j=0; j<super_block.blocks_per_group/8; j++){
            block_bitmap_arr[i][j] = 0;
        }
    }

    for(int i=0; i<bgd_t.size(); i++){
        ext2_block_group_descriptor bgd = bgd_t[i];
        //print_group_descriptor(&bgd);
        int group_start = i*super_block.blocks_per_group;
        int group_end = bgd.inode_table + ceil((super_block.inodes_per_group * super_block.inode_size) / block_size);
        // if(block_size <= 1024){
        //     group_start += 1;
        //     group_end += 1;
        // } 
        char block_bitmap[super_block.blocks_per_group/8];
        lseek(image, bgd.block_bitmap * block_size, SEEK_SET);
        read(image, block_bitmap, sizeof(block_bitmap));

        for(int j=0; j<sizeof(block_bitmap); j++){
            for(int k=0; k<8; k++){
                char cur_block[block_size];
                int cur_block_index = i*super_block.blocks_per_group + j*8 + k; // Bu +1 muhabbeti sikintili
                if(block_size <= 1024){
                    cur_block_index += 1;
                }
                lseek(image, cur_block_index * block_size, SEEK_SET);
                read(image, cur_block, sizeof(cur_block));

                for(int l=0; l<block_size; l++){
                    if(cur_block[l] != 0){
                        block_bitmap[j] |= (1<<k);
                        break;
                    }
                    if (cur_block_index >= group_start && cur_block_index < group_end) {
                        block_bitmap[j] |= (1<<k);
                        break;
                    }
                }

            }
            block_bitmap_arr[i][j] = block_bitmap[j];
        }

        //block_bitmap_arr[i] = block_bitmap; calisir mi ki


        // lseek(image, bgd.block_bitmap * block_size, SEEK_SET);
        // if (write(image, block_bitmap, sizeof(block_bitmap)) != sizeof(block_bitmap)) {
        //     perror("write");
        //     // handle error
        // }
    }

    traverse_inodes();

    for(int i=0; i<bgd_t.size(); i++){
        ext2_block_group_descriptor bgd = bgd_t[i];
        lseek(image, bgd.block_bitmap * block_size, SEEK_SET);
        if (write(image, &(block_bitmap_arr[i][0]), super_block.blocks_per_group/8) != super_block.blocks_per_group/8) {
            perror("write");
            // handle error
        }
    }
    //olmayan bloga yaziyor testcases1 icin
    

}

int main(int argc, char* argv[]){
    /* Open ext2 image file */
    if ((image = open(argv[1], O_RDWR)) < 0) {
        fprintf(stderr, "Could not open image\n");
        return 1;
    }

    // Get super block
    lseek(image, EXT2_SUPER_BLOCK_POSITION, SEEK_SET);
    read(image, &super_block, sizeof(super_block));
    //print_super_block(&super_block);
    
    //cout << "inode count: " << super_block.inode_count << " inodes per group: " << super_block.inodes_per_group << endl;

    bgd_t.resize((super_block.inode_count + super_block.inodes_per_group - 1) / super_block.inodes_per_group); // -1 yapmali miyim emin degilim

    block_size = EXT2_UNLOG(super_block.log_block_size);

    // Get block group descriptor table
    //lseek(image, EXT2_SUPER_BLOCK_POSITION + EXT2_SUPER_BLOCK_SIZE, SEEK_SET); //instead of this line I must use the below code:
    if(block_size <= 2048) {
        lseek(image, 2048, SEEK_SET);
    } 
    else {
        lseek(image, block_size, SEEK_SET);
    }

    read(image, &(bgd_t[0]), bgd_t.size() * sizeof(ext2_block_group_descriptor));
    //print_group_descriptor(&bgd_t[0]);

    // Get root 
    root = get_inode(EXT2_ROOT_INODE);

    print_tree();

    recover_inode_bitmap();
    recover_block_bitmap();

    close(image);
    return 0;
}


/*
inode mode 0 ise bitmap 0
inode deletion_time 0 ise bitmap 1
inode link_count 0 degil ise bitmap 1
*/