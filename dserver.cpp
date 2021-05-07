// Server side C/C++ program to demonstrate a disk-storage system
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>

//PORT 1333 for server & client 
#define SIZE 1024
char buf[SIZE];
#define BS 128

#define PORT 1333
int track, sec;
int fd;
char diskfile[SIZE];
char reader[BS + 1];

//Defines the block of 128 characters
struct block
{
    char val[BS];
};
struct block *blkmap;

int main(int argc, char *argv[])
{
    int sockfd, client_sockfd;
    int nread;
    socklen_t len;
    struct sockaddr_in serv_addr, client_addr;
    uint16_t serv_port;

    int i, j, result, tk, sc, inlen, num;
    char cmd;
    char *bmap, *info;

    char *cmdError;
    cmdError = (char *)malloc(sizeof(char) * SIZE);
    info = (char *)malloc(sizeof(char) * SIZE);
    strcpy(cmdError, "Command Error\n\n Use the correct format:\n\t1. I\n\t2. R <track #> <sector #>\n\t3. W <track #> <sector #> <InputLength> <data>\n\t4. Exit");
    off_t readfrm = 0, writeto = 0;
    int totblk = 0, bin = 0;

    //Checking for the Command Line Arguments
    if ((argc != 5))
    {
        fprintf(stderr, "\nFormat: ./dserver <PORT> <Track> <Sectors> <filename> \n\n");
        exit(1);
    }
    serv_port = atoi(argv[1]);
    track = atoi(argv[2]);
    sec = atoi(argv[3]);
    strncpy(diskfile, argv[4], SIZE - 1);
    long int fsize = track * sec * BS;
    totblk = track * sec;

    printf("\n Port\t: %d", serv_port);
    printf("\n File\t: %s", diskfile);
    printf("\n Track\t: %d", track);
    printf("\n Sector\t: %d", sec);
    printf("\n Blocks\t: %d", totblk);
    printf("\n FSize\t: %ld", fsize);

    // CREATE FILE
    fd = open(diskfile, O_RDWR | O_CREAT, (mode_t)0600);
    if (fd == -1)
    {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }
    // lseek 2 expand the file
    result = lseek(fd, fsize - 1, SEEK_SET);
    if (result == -1)
    {
        close(fd);
        perror("Error");
        exit(EXIT_FAILURE);
    }
    //Writing a last byte of the file
    result = write(fd, "", 1);
    if (result != 1)
    {
        close(fd);
        perror("Error writing last byte");
        exit(EXIT_FAILURE);
    }
    lseek(fd, 0, SEEK_SET);

    //ENDPOINT 4 socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror(NULL);
        exit(2);
    }

    // BINDING ADDRESS
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(serv_port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror(NULL);
        exit(3);
    }

    // QUEUE
    listen(sockfd, 5);

    for (;;)
    {
        printf("\nListening socket for incoming Connections ..\n");
        len = sizeof(client_addr);
        client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
        printf("\nAccepting the connection successfully...\n");
        if (client_sockfd == -1)
        {
            perror(NULL);
            continue;
        }

        printf("\nDserver waiting to read from new Connection...\n");

        //Start reading from the client socket.
        //Here the client starts sending commands to the disk server
        while (1)
        {
            tk = sc = inlen = num = 0;
            memset(buf, '\0', strlen(buf));
            memset(info, '\0', strlen(info));

            memset(reader, '\0', strlen(reader));
            nread = read(client_sockfd, buf, SIZE);
            if (nread == 0)
            {
                printf("\nClosing Client\n");
                break;
            }

            buf[nread] = '\0';
            printf("\nRequest\t: %s\n", buf);
            sscanf(buf, "%c %d %d %d %n", &cmd, &tk, &sc, &inlen, &num);
            cmd = toupper(cmd);

            switch (cmd)
            {
            case 'I': // Sends the information about tracks and sector
                sprintf(info, "%d %d", track, sec);
                len = strlen(info);
                info[len] = '\0';
                printf("Info : %s\n", info);
                write(client_sockfd, info, strlen(info));
                break;
               
            case 'R':
                if (tk > 0 && sc > 0)
                {

                    readfrm = (tk - 1) * sec + (sc - 1);
                    printf("Read from Block : %jd\n", (intmax_t)readfrm);
                    sprintf(info, "%jd", (intmax_t)readfrm);

                    blkmap = (struct block *)mmap(0, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                    if (blkmap == MAP_FAILED)
                    {
                        close(fd);
                        perror("Error mmapping the file");
                        exit(EXIT_FAILURE);
                    }
                    memcpy(reader + 1, blkmap[readfrm].val, BS);

                    if (readfrm < totblk)
                    {
                        reader[0] = '1';
                       
                    }
                    else
                    {
                        reader[0] = '0';
                    }
                    if (munmap(blkmap, fsize) == -1)
                    {
                        perror("Error");
    
                    }
                    write(client_sockfd, reader, BS + 1);
                }
                else
                {
                    bin = 0;
                    sprintf(info, "%d : Incorrect Format : r <track> <sector>", bin);
                    write(client_sockfd, info, strlen(info));
                }
                break;
               

            case 'W': // Write Case File
                if (tk > 0 && tk <= track && sc > 0 && sc <= sec)
                {
                    writeto = (tk - 1) * sec + (sc - 1);
                    printf("Write in Block : %jd\n", (intmax_t)writeto);
                    printf("\nThe buffer starts at : %d\n", num);
                    blkmap = (struct block *)mmap(0, fsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                    if (blkmap == MAP_FAILED)
                    {
                        close(fd);
                        perror("Error mmapping the file");
                        exit(EXIT_FAILURE);
                    }
                    memset(blkmap[writeto].val, '\0', BS);
                    memcpy(blkmap[writeto].val, buf + num, inlen);
                    bin = 1;
                    sprintf(info, "%d", bin);
                    write(client_sockfd, info, strlen(info));
                    if (munmap(blkmap, fsize) == -1)
                    {
                        perror("Error un-mmapping the file");
                        //Decide here whether to close(fd) and exit() or not. Depends...
                    }
                }
                else
                {
                    bin = 0;
                    sprintf(info, "%d : Incorrect Format : w <track#(1-%d)> <Sec#(1-%d)> <len> <data>", bin, track, sec);
                    write(client_sockfd, info, strlen(info));
                }
                break;

            default:
            
                write(client_sockfd, cmdError, strlen(cmdError));
            }
        }
        close(client_sockfd);
    }
    free(cmdError);
    free(info);
}
