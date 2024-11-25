#include <iostream>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <fstream>
#include <iostream>

#ifndef WIN32
#include <dirent.h>
#endif

#include "huffman.h"

struct ersel{   //this structure will be used to create the translation tree
    ersel *left,*right;
    long int number;
    unsigned char character;
    std::string bit;
};

struct translation{
    translation *zero,*one;
    unsigned char character;
};

bool erselcompare0(ersel a,ersel b){
    return a.number<b.number;
}

const static unsigned char check=0b10000000;

//below function is used for writing the uChar to compressed file
//It does not write it directly as one byte instead it mixes uChar and current byte, writes 8 bits of it 
//and puts the rest to curent byte for later use
void write_from_uChar(unsigned char uChar,unsigned char &current_byte,int current_bit_count, std::stringstream& ss){
    current_byte<<=8-current_bit_count;
    current_byte|=(uChar>>current_bit_count);
    ss.write(reinterpret_cast<const char*>(&current_byte), sizeof(current_byte)); 
    current_byte=uChar;   
}

//below function is writing number of files we re going to translate inside current folder to compressed file's 2 bytes
//It is done like this to make sure that it can work on little, big or middle-endian systems
void write_file_count(int file_count,unsigned char &current_byte,int current_bit_count,std::stringstream& ss){
    unsigned char temp=file_count%256;
    write_from_uChar(temp,current_byte,current_bit_count,ss);
    temp=file_count/256;
    write_from_uChar(temp,current_byte,current_bit_count,ss);
}

//This function is writing byte count of current input file to compressed file using 8 bytes
//It is done like this to make sure that it can work on little, big or middle-endian systems
void write_file_size(long int size,unsigned char &current_byte,int current_bit_count,std::stringstream& ss){
    for(int i=0;i<8;i++){
        write_from_uChar(size%256,current_byte,current_bit_count,ss);
        size/=256;
    }
}

// Below function translates and writes bytes from current input file to the compressed file.
void write_the_file_content(const std::string& text, std::string *str_arr, unsigned char &current_byte, int &current_bit_count, std::stringstream& ss){
    unsigned char x;
    char *str_pointer;
    long size = text.length();
    x = text.at(0);
    for(long int i=0;i<size;i++){
        str_pointer=&str_arr[x][0];
        while(*str_pointer){
            if(current_bit_count==8){
                ss.write(reinterpret_cast<const char*>(&current_byte), sizeof(current_byte)); 
                current_bit_count=0;
            }
            switch(*str_pointer){
                case '1':current_byte<<=1;current_byte|=1;current_bit_count++;break;
                case '0':current_byte<<=1;current_bit_count++;break;
                default: std::cout<<"An error has occurred"<< std::endl <<"Process has been aborted";
                exit(2);
            }
            str_pointer++;
        }
        if(i != size - 1) {
            x = (unsigned char)text.at(i + 1);
        }
    }
}

//checks if next input is either a file or a folder
//returns 1 if it is a file
//returns 0 if it is a folder
bool this_is_a_file(unsigned char &current_byte,int &current_bit_count, std::stringstream& ss){
    bool val;
    if(current_bit_count==0){
        ss.read((char*)&current_byte, 1);
        current_bit_count=8;
    }
    val=current_byte&check;
    current_byte<<=1;
    current_bit_count--;
    return val;
}

// process_8_bits_NUMBER reads 8 successive bits from compressed file
//(does not have to be in the same byte)
// and returns it in unsigned char form
unsigned char process_8_bits_NUMBER(unsigned char &current_byte,int current_bit_count, std::stringstream& ss){
    unsigned char val,temp_byte;
    ss.read((char*)&temp_byte, 1);
    val=current_byte|(temp_byte>>current_bit_count);
    current_byte=temp_byte<<8-current_bit_count;
    return val;
}

// returns file's size
long int read_file_size(unsigned char &current_byte,int current_bit_count, std::stringstream& ss){
    long int size=0;
    {
        long int multiplier=1;
        for(int i=0;i<8;i++){
            size+=process_8_bits_NUMBER(current_byte,current_bit_count,ss)*multiplier;
            multiplier*=256;
        }
    }
    return size;
    // Size was written to the compressed file from least significiant byte 
    // to the most significiant byte to make sure system's endianness
    // does not affect the process and that is why we are processing size information like this
}


// This function translates compressed file from info that is now stored in the translation tree
    // then writes it to a newly created file
void translate_file(long int size,unsigned char &current_byte,int &current_bit_count,translation *root, std::stringstream& ss, std::string& text){
    translation *node;
    for(long int i=0;i<size;i++){
        node=root;
        while(node->zero||node->one){
            if(current_bit_count==0){
                ss.read((char*)&current_byte, 1);
                current_bit_count=8;
            }
            if(current_byte&check){
                node=node->one;
            }
            else{
                node=node->zero;
            }
            current_byte<<=1;           
            current_bit_count--;
        }
        text.at(i) = node->character;
    }
}


// process_n_bits_TO_STRING function reads n successive bits from the compressed file
// and stores it in a leaf of the translation tree,
// after creating that leaf and sometimes after creating nodes that are binding that leaf to the tree.
void process_n_bits_TO_STRING(unsigned char &current_byte,int n,int &current_bit_count,std::stringstream& ss,translation *node,unsigned char uChar){
    for(int i=0;i<n;i++){
        if(current_bit_count==0){
            ss.read((char*)&current_byte, 1);
            current_bit_count=8;
        }

        switch(current_byte&check){
            case 0:
            if(!(node->zero)){
                node->zero=(translation*)malloc(sizeof(translation));
                node->zero->zero=NULL;
                node->zero->one=NULL;
            }
            node=node->zero;
            break;
            case 128:
            if(!(node->one)){
                node->one=(translation*)malloc(sizeof(translation));
                node->one->zero=NULL;
                node->one->one=NULL;
            }
            node=node->one;
            break;
        }
        current_byte<<=1;
        current_bit_count--;
    }
    node->character=uChar;
}

// burn_tree function is used for deallocating translation tree
void burn_tree(translation *node){
    if(node->zero)burn_tree(node->zero);
    if(node->one)burn_tree(node->one);
    free(node);
}

//////////////////////////////////////////////////////////////////////

bool TextHuffmanCompression(const std::string& text, std::string& result)
{
    unsigned char x;                  //these are temp variables to take input from the file
    long int total_size=0,size;

    std::stringstream ss;

    long int number[256];
    long int total_bits=0;
    unsigned char letter_count=0;
    for(long int *i=number;i<number+256;i++){                       
        *i=0;
    }

    total_bits+=16+9;
    
    size = text.length();
    total_size += size;
    total_bits+=64;

    x = text.at(0);
    for(long int j=0;j<size;j++){    //counting usage frequency of unique bytes inside the file
        number[x]++;
        x = text.at(j);
    }

	for(long int *i=number;i<number+256;i++){                 
        if(*i){
			letter_count++;
		}
    }
    //---------------------------------------------


    // creating the base of translation array(and then sorting them by ascending frequencies
    // this array of type 'ersel' will not be used after calculating transformed versions of every unique byte
    // instead its info will be written in a new string array called str_arr 
    ersel* array = new ersel[letter_count*2-1];
    ersel *e=array;
    for(long int *i=number;i<number+256;i++){                         
        	if(*i){
                e->right=NULL;
                e->left=NULL;
                e->number=*i;
                e->character=i-number;
                e++;
            }
    }
    std::sort(array,array+letter_count,erselcompare0);
    //---------------------------------------------
    
    // min1 and min2 represents nodes that has minimum weights
    // isleaf is the pointer that traverses through leafs and
    // notleaf is the pointer that traverses through nodes that are not leafs
    ersel *min1=array,*min2=array+1,*current=array+letter_count,*notleaf=array+letter_count,*isleaf=array+2;            
    for(int i=0;i<letter_count-1;i++){                           
        current->number=min1->number+min2->number;
        current->left=min1;
        current->right=min2;
        min1->bit="1";
        min2->bit="0";     
        current++;
        
        if(isleaf>=array+letter_count){
            min1=notleaf;
            notleaf++;
        }
        else{
            if(isleaf->number<notleaf->number){
                min1=isleaf;
                isleaf++;
            }
            else{
                min1=notleaf;
                notleaf++;
            }
        }
        
        if(isleaf>=array+letter_count){
            min2=notleaf;
            notleaf++;
        }
        else if(notleaf>=current){
            min2=isleaf;
            isleaf++;
        }
        else{
            if(isleaf->number<notleaf->number){
                min2=isleaf;
                isleaf++;
            }
            else{
                min2=notleaf;
                notleaf++;
            }
        }
        
    }
    
    for(e=array+letter_count*2-2;e>array-1;e--){
        if(e->left){
            e->left->bit=e->bit+e->left->bit;
        }
        if(e->right){
            e->right->bit=e->bit+e->right->bit;
        }
        
    }

    // In this block we are adding the bytes from root to leafs
    // and after this is done every leaf will have a transformation string that corresponds to it
    // Note: It is actually a very neat process. Using 4th and 5th code blocks, we are making sure that
    // the most used character is using least number of bits.
    // Specific number of bits we re going to use for that character is determined by weight distribution
    //---------------------------------------------

    int current_bit_count=0;
    unsigned char current_byte;
    ss.write(reinterpret_cast<const char*>(&letter_count), sizeof(letter_count)); 
    total_bits+=8;
    //----------------------------------------

    char *str_pointer;
    unsigned char len,current_character;
    std::string str_arr[256];
    for(e=array;e<array+letter_count;e++){
        str_arr[(e->character)]=e->bit;     //we are putting the transformation string to str_arr array to make the compression process more time efficient
        len=e->bit.length();
        current_character=e->character;

        write_from_uChar(current_character,current_byte,current_bit_count,ss);
        write_from_uChar(len,current_byte,current_bit_count,ss);

        total_bits+=len+16;
        // above lines will write the byte and the number of bits
        // we re going to need to represent this specific byte's transformated version
        // after here we are going to write the transformed version of the number bit by bit.
        
        str_pointer=&e->bit[0];
        while(*str_pointer){
            if(current_bit_count==8){
                ss.write(reinterpret_cast<const char*>(&current_byte), sizeof(current_byte)); 
                current_bit_count=0;
            }
            switch(*str_pointer){
                case '1':current_byte<<=1;current_byte|=1;current_bit_count++;break;
                case '0':current_byte<<=1;current_bit_count++;break;
                default:std::cout<<"An error has occurred"<<std::endl<<"Compression process aborted"<<std::endl;
                return false;
            }
           str_pointer++;
        }
        
         total_bits+=len*(e->number);
    }
    if(total_bits%8){
        total_bits=(total_bits/8+1)*8;        
        // from this point on total bits doesnt represent total bits
        // instead it represents 8*number_of_bytes we are gonna use on our compressed file
    }

    delete[]array;
    // Above loop writes the translation script into compressed file and the str_arr array
    //----------------------------------------


    std::cout<<"The size of the sum of ORIGINAL files is: "<<total_size<<" bytes"<<std::endl;
    std::cout<<"The size of the COMPRESSED file will be: "<<total_bits/8<<" bytes"<<std::endl;
    std::cout<<"Compressed file's size will be [%"<<100*((float)total_bits/8/total_size)<<"] of the original file"<<std::endl;
    if(total_bits/8>total_size){
        std::cout<<std::endl<<"COMPRESSED FILE'S SIZE WILL BE HIGHER THAN THE SUM OF ORIGINALS"<<std::endl<<std::endl;
    }

    //-------------writes fourth---------------
    write_file_count(1,current_byte,current_bit_count,ss);
    //---------------------------------------

    
    //-------------writes fifth--------------
    if(current_bit_count==8){
        ss.write(reinterpret_cast<const char*>(&current_byte), sizeof(current_byte)); 
        current_bit_count=0;
    }
    current_byte<<=1;
    current_byte|=1;
    current_bit_count++;
    write_file_size(size,current_byte,current_bit_count,ss);             //writes sixth
    write_the_file_content(text,str_arr,current_byte,current_bit_count,ss);      //writes eighth

    if(current_bit_count==8){      // here we are writing the last byte of the file
        ss.write(reinterpret_cast<const char*>(&current_byte), sizeof(current_byte)); 
    }
    else{
        current_byte<<=8-current_bit_count;
        ss.write(reinterpret_cast<const char*>(&current_byte), sizeof(current_byte)); 
    }

    result = ss.str();
    
    return true;
}

bool TextHuffmanDecompression(const std::string& huffman, std::string& text)
{
    unsigned char letter_count=0;
    std::stringstream ss(huffman);

    //---------reads .first-----------
    ss.read((char*)&letter_count, 1);

    int m_letter_count;
    if(letter_count==0)
        m_letter_count=256;
    else
        m_letter_count = letter_count;
    //-------------------------------

    //----------------reads .second---------------------
        // and stores transformation info into binary translation tree for later use
    unsigned char current_byte=0,current_character;
    int current_bit_count=0,len;
    translation *root=(translation*)malloc(sizeof(translation));
    root->zero=NULL;
    root->one=NULL;

    for(int i=0;i<m_letter_count;i++){
        current_character=process_8_bits_NUMBER(current_byte,current_bit_count,ss);
        len=process_8_bits_NUMBER(current_byte,current_bit_count,ss);

        if(len==0)len=256;
        process_n_bits_TO_STRING(current_byte,len,current_bit_count,ss,root,current_character);
    }
    //--------------------------------------------------



    // ---------reads .third----------
    //reads how many folders/files the program is going to create inside the main folder
    int file_count;
    file_count=process_8_bits_NUMBER(current_byte,current_bit_count,ss);
    file_count+=256*process_8_bits_NUMBER(current_byte,current_bit_count,ss);
    if(file_count != 1) {
        //
        return false;
    }

    // File count was written to the compressed file from least significiant byte 
    // to most significiant byte to make sure system's endianness
    // does not affect the process and that is why we are processing size information like this
    if(this_is_a_file(current_byte,current_bit_count,ss)){   // reads .fifth and goes inside if this is a file
        long int size=read_file_size(current_byte,current_bit_count,ss);  // reads .sixth
        text.resize(size);
        translate_file(size,current_byte,current_bit_count,root,ss, text); //translates .eighth

        burn_tree(root);
        return true;
    }

    burn_tree(root);
    return false;
}
