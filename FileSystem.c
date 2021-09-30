#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>



/***********************************************************
			     Semestralni prace ZOS 2021
			    	Author: Petr Tomsik
*************************************************************/

#define FILENF "FILE NOT FOUND\n"
#define PATHNF "PATH NOT FOUND\n"
#define FITL "FILE IS TOO LARGE"
#define EXIST "EXIST\n"
#define NE  "NOT EMPTY\n"
#define OK  "OK\n"
#define CCF "CANNOT CREATE FILE\n"
#define NES "FILESYSTEM HAS NOT ENOUGH SPACE\n"


#define BUFF_SIZE 256				/* Velikost vyrovnávací paměti pro vstupní příkazy */
#define CLUSTER_SIZE 4096			/* Velikost jednoho klastru v bajtech */
#define MAX_NUMBERS_IN_BLOCK 256	/* Počet čísel (celá čísla) v jednom bloku */
#define MIN_FS_SIZE 20480			/* Minimální velikost souborového systému */
#define MAX_SIZE 7340032			/* Maximální velikost souboru, který lze uložit do souborového systému (4136 * 1024) */ /* 7340032 pět přímých odkazů , dva nepřímé 5+2*256*4096B */
#define INODE_SIZE 38				/* Velikost i-uzlu v bajtech */
#define ERROR -1                    /* Hodnota pro chybu */
#define NO_ERROR 0

/*-------------------------------------------------------------------------------------------*/

// Struktura i-uzlu
typedef struct theinode
{
    int8_t isDirectory;             /* 0 = soubor, 1 = adresář */
    int8_t references;              /* Počet odkazů na i-uzel */
    int32_t nodeid;                 /* ID i-uzlu, pokud ID = ZDARMA, pak je i-uzel zdarma */
    int32_t file_size;              /* Velikost souboru / adresáře v bajtech */
    int32_t direct1;                /* 1. přímý odkaz na datové bloky */
    int32_t direct2;                /* 2. přímý odkaz na datové bloky */
    int32_t direct3;                /* 3. přímý odkaz na datové bloky */
    int32_t direct4;                /* 4. přímý odkaz na datové bloky */
    int32_t direct5;                /* 5. přímý odkaz na datové bloky */
    int32_t indirect1;              /* 1. nepřímý odkaz */
    int32_t indirect2;              /* 2. nepřímý odkaz */
} inode;

// Struktura superbloku
typedef struct thesuperblock{
    int32_t disk_size;              /* Velikost souborového systému */
    int32_t bitmap_start_address;   /* Počáteční adresa bitmapy datových bloků */
    int32_t inode_start_address;    /* Počáteční adresa i-uzlů */
    int32_t data_start_address;     /* Počáteční adresa datových bloků */
    int32_t inode_count;			/* Počet i-uzlů */
    int32_t bitmap_cluster_count;	/* Počet klastrů pro bitmapu */
    int32_t data_cluster_count;		/* Počet klastrů pro data */
    int32_t cluster_size;           /* Velikost shluku */
    int32_t cluster_count;          /* Počet klastrů */
    int32_t inode_cluster_count;	/* Počet klastrů pro i-uzly */
}superblock;

// Struktura položky adresáře
typedef struct thedirectory_item
{
    int32_t inode;               	/* ID i-uzlu (index do pole) */
    char item_name[12];             /* Název souboru 8 + 3 + \ 0 */
    struct thedirectory_item *next;	/* Odkaz na jinou položku adresáře v aktuálním adresáři */
}   directory_item;

// Struktura adresáře
typedef struct thedirectory
{
    struct thedirectory *parent;	/* Odkaz na nadřazený adresář */
    directory_item *current;		/* Aktuální položka adresáře */
    directory_item *subdir;			/* Reference to the first subdirectory in the list of all subdirectories in the current directory */
    directory_item *file;			/* Reference to the first file in the list of all files in the current directory */
} directory;

// Struktura informací. o konkrétním datovém bloku
typedef struct thedata_info {
    int32_t nodeid;				/* ID I-uzlu, které obsahuje tento datový blok */
    int32_t *ref_addr;			/* Adresa přímé / nepřímé reference, kde je tento datový blok uložen */
    int32_t indir_block;		/* Počet datových bloků nepřímé reference */
    int32_t order_in_block;		/* Umístění v nepřímém datovém bloku (pořadí čísel) */
} data_info;

/*-------------------------------------------------------------------------------------------*/

void runProgram();
void freeMemory();
void format(long bytes);
FILE *load(char *file);
void cd(char *path);
void ls(char *path);
void cp(char *files);
void rm(char *file);
void mv(char *files);
void pwd();
void mymkdir(char *path);
void myrmdir(char *path);
void info(char *file);
void incp(char *files);
void cat(char *file);
void outcp(char *files);
void ln(char *files);
void slink(char *files);

data_info *create_data_info(int32_t nodeid, int32_t *ref_addr, int32_t indir_block, int32_t order_in_block);
int32_t get_size(char *size);
int32_t find_free_inode();
int32_t *find_free_data_blocks(int count);
int parse_path(char *path, char **name, directory **dir);
directory_item *find_item(directory_item *first_item, char *name);
int32_t *get_data_blocks(int32_t nodeid, int *block_count, int *rest);
int create_directory(directory *parent, char *name);
int test_existence(directory *dir, char *name);
directory_item *create_directory_item(int32_t inode_id, char *name);
directory *find_directory(char *path, char **name);
void initialize_inode(int32_t id, int32_t size, int block_count, int tmp_count, int *last_block_index, int32_t *blocks);
void free_directories(directory *root);
void clear_inode(int id);
void update_sizes(directory *dir, int32_t size);
void print_info(directory_item *item);
void print_file(directory_item *item);
void print_format_msg();
void load_fs();
void load_directory(directory *dir, int id);
void update_bitmap(directory_item *item, int8_t value, int32_t *data_blocks, int b_count);/*Bitmapa i-nodů říká, který i-node je volný*/
void update_inode(int id);
int update_directory(directory *dir, directory_item *item, int action);
void remove_reference(directory_item *item, int32_t block_id);

/*-------------------------------------------------------------------------------------------*/


const int32_t FREE = -1;				/* Položka je volná */
char *fs_name;							/* Název souborového systému */
FILE *fs;								/* Soubor se souborovým systémem */
superblock *sb;					        /* Superblock */
int8_t *bitmap = NULL;					/* Bitmapa datových bloků, 0 = volný 1 = plný */
inode *inodes = NULL;					/* Pole i-uzlů, ID i-uzlu = index do pole */
directory **directories = NULL;			/* Pole ukazatelů na adresáře, ID i-uzlu = index do pole */
directory *working_directory;			/* Aktuální adresář */
int fs_formatted;						/* Pokud je souborový systém naformátován, 0 = false, 1 = true */
char block_buffer[CLUSTER_SIZE];		/* Buffer (vyrovnávací paměť) pro jeden klastr */
int file_input = 0;						/* Pokud jsou příkazy načteny ze souboru */
char compared[16];                      /* Řetězec pro porovnání */
const char *DELIM = " \n";

/*-------------------------------------------------------------------------------------------*/



/*
 * Vstupní bod programu param argv [1] ... název souborového systému
*/
int main(int argc, char *argv[]){
    if (argc < 2) {
        printf("No argument! Enter the filesystem name. \n");
        return EXIT_FAILURE;
    }

    printf("Filesystem is running  ... \n");
    fs_name = argv[1];

    // Test if filesystem already exists
    if (access(argv[1], F_OK) == -1) {
        fs_formatted = 0;
        printf("The filesystem has to be formatted first. \nUsage: format [size] \n");
    }
    else {
        fs_formatted = 1;
        load_fs();
    }

    runProgram();
    freeMemory();

    return EXIT_SUCCESS;  /* Vyhodí chybu */
}


/*
 * Zpracovat uživatelské příkazy
 */
void runProgram(){
    char *cmd, *args;
    char buffer[BUFF_SIZE];	        /* Buffer (Vyrovnávací paměť) uživatelských příkazů */
    FILE *f;				        /* Soubor, ze kterého lze načíst příkazy místo konzoly */
    int32_t fs_size;		        /* Velikost souborového systému */
    short exit = 0;


    do{
        memset(buffer, 0, BUFF_SIZE);


        if (file_input){		     /* Příkazy ze souboru */
            fgets(buffer, BUFF_SIZE, f);
            if (!feof(f)){
                printf("%s", buffer);
            }
            else{ 				    /* Přepněte zpět na příkazy z konzoly */
                file_input = 0;
                fclose(f);
                continue;
            }
        }

        else{					/* Příkazy z konzoly */
            fgets(buffer, BUFF_SIZE, stdin);
        }

        if (buffer[0] == '\n')	/* Přeskočit prázdný řádek */
            continue;


        cmd = strtok(buffer, DELIM);
        args = strtok(NULL, "\n");

        if (strcmp("cp", cmd) == 0){
            cp(args);
        }
        else if (strcmp("mv", cmd) == 0){
            mv(args);
        }
        else if (strcmp("rm", cmd) == 0){
            rm(args);
        }
        else if (strcmp("mkdir", cmd) == 0){
            mymkdir(args);
        }
        else if (strcmp("rmdir", cmd) == 0){
            myrmdir(args);
        }
        else if (strcmp("ls", cmd) == 0){
            ls(args);
        }
        else if (strcmp("cat", cmd) == 0){
            cat(args);
        }
        else if (strcmp("cd", cmd) == 0){
            cd(args);
        }
        else if (strcmp("pwd", cmd) == 0){
            pwd();
        }
        else if (strcmp("info", cmd) == 0){
            info(args);
        }
        else if (strcmp("incp", cmd) == 0){
            incp(args);
        }
        else if (strcmp("outcp", cmd) == 0){
            outcp(args);
        }
        else if (strcmp("load", cmd) == 0){
            f = load(args);
        }
        else if (strcmp("ln", cmd) == 0){
            ln(args);
          //check();
        }
        else if (strcmp("slink", cmd) == 0){
            slink(args);
            //dis();
        }
        else if (strcmp("format", cmd) == 0){
            fs_size = get_size(args);
            if (fs_size == ERROR)		/* Problém s velikostí souborového systému */
                continue;
            format(fs_size);
        }
        else if (buffer[0] == 'e' || buffer[0] == 'E' ){	    /* Ukončující příkazy (end) */
            exit = 1;
        }
        else{
            printf(" UNKNOWN COMMAND, try it again \n");
        }
    }while (!exit);
}



/*
 * Před ukončením programu proveďte všechny potřebné operace
 */
void freeMemory(){
    if (sb){
        free(sb);
    }
    if (bitmap){
        free(bitmap);
    }
    if (inodes){
        free(inodes);
    }
    if (directories){
        free_directories(directories[0]);
        free(directories);
    }
    fclose(fs);
}



/*
 * Naformátuje stávající souborový systém nebo vytvořtí nový se specifickou velikostí
 * param bytes ... velikost souborového systému v bytech
 */
void format(long bytes){
    int i, one = 1;
    directory *root;

    if (!fs){
        fs = fopen(fs_name, "wb+");
    }

    /* Připrví superblok */
    if (!fs_formatted){		/* Pokud neexistuje -> vytvořit */
        sb = (superblock *)malloc(sizeof(superblock));
        if (!sb){
            printf(CCF);
            return;
        }
    }

    sb->cluster_size = CLUSTER_SIZE;											/* Velikost klastru */
    sb->cluster_count = bytes / CLUSTER_SIZE; 									/* Počet všech klastrů */
    sb->disk_size = sb->cluster_count * CLUSTER_SIZE; 							/* Přesná velikost souborového systému v bajtech */
    sb->inode_cluster_count = sb->cluster_count / 20; 							/* Počet bloků pro i-uzly, 5% všech bloků */
    sb->inode_count = (sb->inode_cluster_count * CLUSTER_SIZE) / INODE_SIZE;	/* Počet i-uzlů */
    sb->bitmap_start_address = CLUSTER_SIZE; 									/* Počáteční adresa bitmapových bloků */
    sb->bitmap_cluster_count = (int)((sb->cluster_count - sb->inode_cluster_count - 1) / (float)CLUSTER_SIZE);	/* Počet bloků pro bitmapu, která pokryje všechny datové bloky */
    sb->data_cluster_count = sb->cluster_count - 1 - sb->bitmap_cluster_count - sb->inode_cluster_count;		/* Počet datových bloků */
    sb->inode_start_address = sb->bitmap_start_address + CLUSTER_SIZE * sb->bitmap_cluster_count;				/* Počáteční adresa bloků i-uzlu */
    sb->data_start_address = sb->inode_start_address + CLUSTER_SIZE * sb->inode_cluster_count;					/* Počáteční adresa datových bloků */


    if (fs_formatted){		/* Jestliže byl souborový systém již naformátován */
        free(bitmap);
        free_directories(directories[0]);
        free(directories);
        free(inodes);
    }

    /* Připraví bitmapu, i-uzly a ukazatele na adresáře */
    bitmap = (int8_t *)malloc(sb->data_cluster_count);
    inodes = (inode *)malloc(sizeof(inode) * sb->inode_count);
    directories = (directory **)malloc(sizeof(directory *) * sb->inode_count);
    if (!bitmap || !inodes || !directories){
        printf(CCF);
        return;
    }

    /* Vytvoří kořenový adresář */
    root = (directory *)malloc(sizeof(directory));
    if (!root){
        printf(CCF);
        return;
    }
    root->current = create_directory_item(0,"/");
    root->parent = root;
    root->subdir = NULL;
    root->file = NULL;

    working_directory = root;
    directories[0] = root;

    bitmap[0] = 1;

    /* Vymaže bitmapu */
    for (i = 1; i < sb->data_cluster_count; i++){
        bitmap[i] = 0;
    }

    /* Nastaví všechny i-uzly jako volné */
    for (i = 0; i < sb->inode_count; i++){
        inodes[i].nodeid = FREE;
        inodes[i].isDirectory = 0;
        inodes[i].references = 0;
        inodes[i].file_size = 0;
        inodes[i].direct1 = FREE;
        inodes[i].direct2 = FREE;
        inodes[i].direct3 = FREE;
        inodes[i].direct4 = FREE;
        inodes[i].direct5 = FREE;
        inodes[i].indirect1 = FREE;
        inodes[i].indirect2 = FREE;
    }

    /* Nastaví kořenový i-uzel */
    inodes[0].nodeid = 0;
    inodes[0].isDirectory = 1;
    inodes[0].references = 1;
    inodes[0].direct1 = 0;

    /* Vyplňí soubor nulami */
    memset(block_buffer, 0, CLUSTER_SIZE);
    for (i = 0; i < sb->cluster_count; i++){
        fwrite(block_buffer, sizeof(block_buffer), 1, fs);
    }

    /* Uloží superblok */
    rewind(fs);
    fwrite(&(sb->disk_size), sizeof(int32_t), 1, fs);
    fwrite(&(sb->cluster_size), sizeof(int32_t), 1, fs);
    fwrite(&(sb->cluster_count), sizeof(int32_t), 1, fs);
    fwrite(&(sb->inode_count), sizeof(int32_t), 1, fs);
    fwrite(&(sb->bitmap_cluster_count), sizeof(int32_t), 1, fs);
    fwrite(&(sb->inode_cluster_count), sizeof(int32_t), 1, fs);
    fwrite(&(sb->data_cluster_count), sizeof(int32_t), 1, fs);
    fwrite(&(sb->bitmap_start_address), sizeof(int32_t), 1, fs);
    fwrite(&(sb->inode_start_address), sizeof(int32_t), 1, fs);
    fwrite(&(sb->data_start_address), sizeof(int32_t), 1, fs);

    fseek(fs, sb->bitmap_start_address, SEEK_SET);
    fwrite(&one, sizeof(int8_t), 1, fs);

    for (i = 0; i < sb->inode_count; i++){
        update_inode(i);
    }

    fs_formatted = 1;
    printf(OK);
}


/*
 * Zkopírujte soubor do jiného adresáře
 * soubory param .. zdrojový soubor a cílový adresář (+ cesty)
*/
void cp(char *files){
    int i, block_count, rest, tmp, count_with_indir, last_block_index;
    char *source, *dest, *name;
    int32_t *source_blocks, *dest_blocks, inode_id;
    directory *source_dir, *dest_dir;
    directory_item *item, **pitem;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!files || files == ""){
        printf(FILENF);
        return;
    }
    source = strtok(files, " ");	        /* Získání zdrojů (odkud) */
    dest = strtok(NULL, "\n");		/* Získaní destinace (kam) */
    if (!dest || dest == ""){
        printf(FILENF);
        return;
    }

    /* Parsuje cestu ke zdroji + najde zdrojový adresář */
    if (parse_path(source, &name, &source_dir)){
        printf(FILENF);
        return;
    }

    /* Najde soubor ve zdrojovém adresáři */
    item = find_item(source_dir->file, name);
    if (!item){
        printf(FILENF);
        return;
    }

    /* Najde cílový adresář */
    dest_dir = find_directory(dest, &name);

    if (!dest_dir){
        printf(PATHNF);
        return;
    }

    /* Test, zda cílová složka obsahuje soubor / adresář se stejným názvem */
    if (test_existence(dest_dir, name)){
        printf(EXIST);
        return;
    }

    /* Získá počet datových bloků zdrojového souboru */
    source_blocks = get_data_blocks(item->inode, &block_count, &rest);

    if (block_count < 5){                                /* Použíje pouze přímé odkazy */
        count_with_indir = block_count;
    }
    else if ((block_count > 5) && (block_count < 262)){    /* Použije první nepřímý odkaz (datový blok +1) */
        count_with_indir = block_count + 1;
    }
    else{
        count_with_indir = block_count + 2;                /* Použíje obě nepřímé reference (+2 datový blok) */
    }
    /* Získá počet volných datových bloků pro zkopírovaný soubor */
    dest_blocks = find_free_data_blocks(count_with_indir);
    if (!dest_blocks){
        printf(NES);
        return;
    }

    /* Získá ID volného i-uzlu */
    inode_id = find_free_inode();
    if (inode_id == ERROR){
        printf(NES);
        return;
    }

    /* Získá poslední (volnou) položku v seznamu všech souborů v cílovém adresáři */
    pitem = &(dest_dir->file);
    while (*pitem != NULL){
        pitem = &((*pitem)->next);
    }
    *pitem = create_directory_item(inode_id, name);

    /* Inicializuje i-uzel */
    initialize_inode(inode_id, inodes[item->inode].file_size, block_count, count_with_indir, &last_block_index, dest_blocks);

    /* Uloží změny do souboru */
    update_bitmap(*pitem, 1, dest_blocks, block_count);
    update_inode(inode_id);
    update_sizes(dest_dir, inodes[item->inode].file_size);
    update_directory(dest_dir, *pitem, 1);

    /* Kopíroje datové bloky */
    for (i = 0; i < block_count - 1; i++){
        fseek(fs, sb->data_start_address + source_blocks[i] * CLUSTER_SIZE, SEEK_SET);
        fflush(fs);
        fread(block_buffer, sizeof(block_buffer), 1, fs);
        fseek(fs, sb->data_start_address + dest_blocks[i] * CLUSTER_SIZE, SEEK_SET);
        fflush(fs);
        fwrite(block_buffer, sizeof(block_buffer), 1, fs);
    }

    /* Zkopíruje poslední datový blok (smí kopírovat pouze část bloku) */
    memset(block_buffer, 0, CLUSTER_SIZE);
    if (rest != 0)
        tmp = rest;
    else
        tmp = CLUSTER_SIZE;

    fseek(fs, sb->data_start_address + source_blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
    fflush(fs);
    fread(block_buffer, tmp, 1, fs);
    fseek(fs, sb->data_start_address + dest_blocks[last_block_index] * CLUSTER_SIZE, SEEK_SET);
    fflush(fs);
    fwrite(block_buffer, tmp, 1, fs);
    fflush(fs);

    free(source_blocks);
    free(dest_blocks);

    printf(OK);
}

/* Odebrat soubor
* soubor param ... odebrání souboru (+ cesta)
*/
void rm(char *file){
    int i, block_count, rest, tmp, prev;
    int32_t *blocks;
    char *name;
    directory *dir;
    directory_item *item, **temp;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!file || file == ""){
        printf(FILENF);
        return;
    }

    /* Parsuje cestu ke zdroji + najde zdrojový adresář */
    if (parse_path(file, &name, &dir)){
        printf(FILENF);
        return;
    }

    /* Odeberte soubor ze seznamu všech souborů v adresáři */
    temp = &(dir->file);
    item = dir->file;
    while (item != NULL){
        if (strcmp(name, item->item_name) == 0){
            (*temp) = item->next;
            break;
        }
        temp = &(item->next);
        item = item->next;
    }

    if (!item){
        printf(FILENF);
        return;
    }

    /* Získá počet datových bloků souboru */
    blocks = get_data_blocks(item->inode, &block_count, &rest);

    /* Vymaže datový bloky */
    memset(block_buffer, 0, CLUSTER_SIZE);
    prev = blocks[0];
    for (i = 0; i < block_count - 1; i++){
        if (prev != blocks[i] - 1){
            fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
        }
        fwrite(block_buffer, sizeof(block_buffer), 1, fs);
        prev = blocks[i];
    }

    if (rest != 0)
        tmp = rest;
    else
        tmp = CLUSTER_SIZE;

    fseek(fs, sb->data_start_address + blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
    fwrite(block_buffer, tmp, 1, fs);

    if (inodes[item->inode].indirect1 != FREE){
        fseek(fs, sb->data_start_address + inodes[item->inode].indirect1 * CLUSTER_SIZE, SEEK_SET);
        fwrite(block_buffer, sizeof(block_buffer), 1, fs);

        if (inodes[item->inode].indirect2 != FREE){
            fseek(fs, sb->data_start_address + inodes[item->inode].indirect2 * CLUSTER_SIZE, SEEK_SET);
            fwrite(block_buffer, sizeof(block_buffer), 1, fs);
        }
    }

    fflush(fs);

    update_bitmap(item, 0, blocks, block_count);
    update_sizes(dir, -(inodes[item->inode].file_size));
    update_directory(dir, item, 0);

    clear_inode(item->inode);
    update_inode(item->inode);

    free(item);
    free(blocks);

    printf(OK);
}


void ln(char *files){
    printf("%s","Hotova");
    char *path1 = "/Petr.txt";
    char *path2 = "/Kamil.txt";
    int   status;
    printf("%s","Hotova");
    status = link (path1, path2);
    printf("%d",status);

/*
    BOOL bHfile;
    bHfile = CreateHardlink(
            L"afs/zcu.cz/users/p/ptomsik/home/Plocha/Petr.txt",
            L"afs/zcu.cz/users/p/ptomsik/home/Plocha/Karel.txt",
            NULL);
    if(bHfile == FALSE){
        count<<"CreateHardlink Failed a Error no = " <<GetLastError()<<endl;

    }
    count<<"CreateHardlink Succes<<end";

*/
}


void slink(char *files){

}

/* Tisk (výpis) všech položek v adresáři
* cesta param ... cesta k adresáři
*/
void ls(char *path){
    directory *dir;
    directory_item *item;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!path || path == ""){
        printf(PATHNF);
        return;
    }

    /* Najde adrešář */
    dir = find_directory(path, NULL);
    if (!dir){
        printf(PATHNF);
        return;
    }

    item = dir->subdir;
    while (item != NULL){	/* Vypíše všechy podadresáře */
        printf("+%s\n", item->item_name);
        item = item->next;
    }

    item = dir->file;
    while (item != NULL){	/* Tisk všech souborů */
        printf("-%s\n", item->item_name);
        item = item->next;
    }
}



/* Vytvořit nový adresář
* cesta param ... název nového adresáře (+ cesta)
*/
void mymkdir(char *path){
    directory *dir;
    char *name;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!path || path == ""){
        printf(PATHNF);
        return;
    }

    /* Parsuje cestu ke zdroji + najde zdrojový adresář */
    if (parse_path(path, &name, &dir)){
        printf(PATHNF);
        return;
    }

    /* Test, zda cílová složka obsahuje soubor / adresář se stejným názvem */
    if (test_existence(dir, name)){
        printf(EXIST);
        return;
    }

    /* Vytvoří adresář */
    if (create_directory(dir, name)){
        printf(NES);
        return;
    }
    printf(OK);
}


/* Odeberte prázdný adresář
* cesta param ... název odstraněného adresáře (+ cesta)
*/
void myrmdir(char *path){
    directory *dir;
    directory_item *item, **temp;
    char *name;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!path || path == ""){
        printf(PATHNF);
        return;
    }

    /* Parsuje cestu ke zdroji + najde zdrojový adresář */
    if (parse_path(path, &name, &dir)){
        printf(PATHNF);
        return;
    }

    temp = &(dir->subdir);
    item = dir->subdir;
    while (item != NULL){
        if (strcmp(name, item->item_name) == 0){
            if ((directories[item->inode]->file != NULL) || (directories[item->inode]->subdir != NULL)) {		// If directory is not empty
                printf(NE);
                return;
            }

            (*temp) = item->next;

            if (working_directory == directories[item->inode]){	// If removing working directory -> get to one level up in hierarchy
                working_directory = directories[item->inode]->parent;
            }

            update_bitmap(item, 0, NULL, 0);
            clear_inode(item->inode);
            update_inode(item->inode);
            update_directory(dir, item, 0);

            free(directories[item->inode]);
            free(item);
            break;
        }
        temp = &(item->next);
        item = item->next;
    }

    if (!item){	// Jestli adresář nebyl nalezen
        printf(FILENF);
        return;
    }

    printf(OK);
}



/*	Přesunout soubor do jiného adresáře
*   soubory param ... zdrojový soubor a cílový adresář (+ cesty)
*/
void mv(char *files){
    char *source, *dest, *name;
    directory *source_dir, *dest_dir;
    directory_item *item, **pitem, **temp;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!files || files == ""){
        printf(FILENF);
        return;
    }

    source = strtok(files, " ");	        /* Získání zdrojů (odkud) */
    dest = strtok(NULL, "\n");		/* Získaní destinace (kam) */
    if (!dest || dest == ""){
        printf(FILENF);
        return;
    }

    /* Parsuje cestu ke zdroji + najde zdrojový adresář */
    if (parse_path(source, &name, &source_dir)){
        printf(FILENF);
        return;
    }

    /* Najde soubor ve zdrojovém adresáři */
    dest_dir = find_directory(dest, &name);

    if (!dest_dir){
        printf(PATHNF);
        return;
    }

    /* Pokud jsou zdrojové a cílové (kam) adresáře stejné */
    if (dest_dir == source_dir){
        printf(OK);
        return;
    }

    /* Test, zda cílová složka obsahuje soubor / adresář se stejným názvem */
    if (test_existence(dest_dir, name)){
        printf(EXIST);
        return;
    }

    //Odebere soubor ze seznamu všech souborů ve zdrojovém adresáři
    temp = &(source_dir->file);
    item = source_dir->file;
    while (item != NULL) {
        if (strcmp(name, item->item_name) == 0) {
            (*temp) = item->next;
            update_sizes(source_dir, -(inodes[item->inode].file_size));
            update_directory(source_dir, item, 0);
            break;
        }
        temp = &(item->next);
        item = item->next;
    }

    if (!item){
        printf(FILENF);
        return;
    }

    /* Získá poslední (volnou) položku v seznamu všech souborů v cílovém (destinace/kam) adresáři */
    pitem = &(dest_dir->file);
    while (*pitem != NULL){
        pitem = &((*pitem)->next);
    }

    *pitem = item;	/* Přidá soubor do cílového adresáře */
    update_sizes(dest_dir, inodes[item->inode].file_size);
    update_directory(dest_dir, item, 1);

    printf(OK);
}


/* Vypíše obsah souboru
* soubor param ... název souboru (+ cesta)
*/
void cat(char *file){
    directory *dir;
    directory_item *item;
    char *name;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!file || file == ""){
        printf(FILENF);
        return;
    }

    /* Parsuje cestu ke zdroji + najde zdrojový adresář */
    if (parse_path(file, &name, &dir)){
        printf(FILENF);
        return;
    }

    item = find_item(dir->file, name);
    if (!item){
        printf(FILENF);
        return;
    }
    print_file(item);
    printf("\n");
}


/* Změňte aktualní adresář podle cesty
 * cesta param ... cesta k novému pracovnímu adresáři
*/
void cd(char *path){
    directory *dir;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!path || path == ""){
        printf(PATHNF);
        return;
    }

    /* Najdi adesář */
    dir = find_directory(path, NULL);
    if (!dir){
        printf(PATHNF);
        return;
    }

    working_directory = dir;
    printf(OK);
}


/*	Vytiskne cestu aktuálního adresáře */
void pwd(){
    char *names[64];	/* Názvy adresářů na cestě k aktuálním adresáři */
    int count = 0;
    int i;
    directory *temp = working_directory;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    while (temp != directories[0]){	/* Dokud se nedostaneme do kořenového adresáře */
        names[count] = temp->current->item_name;
        count++;
        temp = temp->parent;
    }

    printf("/");
    for (i = count - 1; i >= 0; i--){
        printf("%s", names[i]);
        if (i != 0)
            printf("/");
    }
    printf("\n");
}



/* Zkopírujte soubor z externího souborového systému do tohoto souborového systému
*  soubory param ... zdrojový soubor a cílový adresář (+ cesty)
*/
void incp(char *files){
    int32_t file_size, *blocks, inode_id;
    int i, block_count, rest, tmp_count, tmp, last_block_index, prev;
    char *source, *dest, *name;
    directory *dir;
    directory_item **pitem;
    FILE *f;


    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!files || files == ""){
        printf(FILENF);
        return;
    }
    source = strtok(files, " ");	        /* Získání zdrojů (odkud) */
    dest = strtok(NULL, "\n");	    /* Získaní destinace (kam) */
    if (!dest || dest == ""){
        printf(PATHNF);
        return;
    }

    /* Získá název zdrojového souboru */
    if ((name = strrchr(source, '/')) == NULL){
        name = source;
    }
    else{
        name++;
    }

    /* Najít cílový adresář */
    dir = find_directory(dest, &name);
    if (!dir) {
        printf(PATHNF);
        return;
    }

    /* Test, zda cílová složka neobsahuje soubor se stejným názvem */
    if (test_existence(dir, name)){
        printf(EXIST);
        return;
    }

    if (!(f = fopen(source, "rb"))){
        printf(FILENF);
        return;
    }

    /* Získá velikost zkopírovaného souboru */
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    rewind(f);

    if (file_size > MAX_SIZE){
        printf(FITL);
        fclose(f);
        return;
    }

    block_count = file_size / CLUSTER_SIZE;
    rest = file_size % CLUSTER_SIZE;

    if (rest != 0){
        block_count++;
    }

    if (block_count < 5){                               /* Použíje pouze přímé odkazy */
        tmp_count = block_count;
    }
    else if ((block_count > 5) && (block_count < 262)){ /* Použije první nepřímý odkaz (datový blok +1) */
        tmp_count = block_count + 1;
    }
    else{
        tmp_count = block_count + 2;                    /* Použíje obě nepřímé reference (+2 datový blok) */
    }
    blocks = find_free_data_blocks(tmp_count);

    if (!blocks){
        printf(NES);
        fclose(f);
        return;
    }
    /* Získá ID volného i-uzlu */
    inode_id = find_free_inode();
    if (inode_id == ERROR){
        printf(NES);
        fclose(f);
        return;
    }

    pitem = &(dir->file);
    while (*pitem != NULL){
        pitem = &((*pitem)->next);
    }
    *pitem = create_directory_item(inode_id, name);

    /* Inicializuje i-uzle */
    initialize_inode(inode_id, file_size, block_count, tmp_count, &last_block_index, blocks);

    update_bitmap(*pitem, 1, blocks, block_count);
    update_inode(inode_id);
    update_directory(dir, *pitem, 1);
    update_sizes(dir, file_size);

    /* Zkopírovaná data */
    prev = blocks[0];

    for (i = 0; i < block_count - 1; i++){
        fread(block_buffer, sizeof(block_buffer), 1, f);
        if (prev != blocks[i] - 1){
            fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
        }
        fwrite(block_buffer, sizeof(block_buffer), 1, fs);
        prev = blocks[i];
    }

    memset(block_buffer, 0, CLUSTER_SIZE);
    if (rest != 0){
        tmp = rest;
    }
    else{
        tmp = CLUSTER_SIZE;
    }

    fread(block_buffer, sizeof(char), tmp, f);
    fseek(fs, sb->data_start_address + blocks[last_block_index] * CLUSTER_SIZE, SEEK_SET);
    fwrite(block_buffer, sizeof(char), tmp, fs);
    fflush(fs);
    fclose(f);
    free(blocks);
    printf(OK);
}


/* Zkopíruje soubor z tohoto souborového systému do externího souborového systému
* soubory param ... zdrojový soubor a cílový adresář (+ cesty)
*/
void outcp(char *files){
    int i, block_count, rest, tmp, prev;
    int32_t *blocks;
    char *source, *dest, *name;
    char whole_dest[BUFF_SIZE];
    directory *dir;
    directory_item *item;
    FILE *f;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!files || files == ""){
        printf(FILENF);
        return;
    }
    source = strtok(files, " ");	        /* Získání zdrojů (odkud) */
    dest = strtok(NULL, "\n");		/* Získaní destinace (kam) */
    if (!dest || dest == "") {
        printf(PATHNF);
        return;
    }
    /* Parsuje cestu ke zdroji + najde zdrojový adresář */
    if (parse_path(source, &name, &dir)){
        printf(FILENF);
        return;
    }

    item = find_item(dir->file, name);
    if (!item) {
        printf(FILENF);
        return;
    }

    /* Nastaví cíl (cesta + název) */
    memset(whole_dest, 0, BUFF_SIZE);
    sprintf(whole_dest, "%s", dest);

    if (!(f = fopen(whole_dest, "wb"))){
        printf(PATHNF);
        return;
    }

    blocks = get_data_blocks(item->inode, &block_count, &rest);

    /* Zkopíruje data */
    prev = blocks[0];
    for (i = 0; i < block_count - 1; i++) {
        if (prev != (blocks[i] - 1)) {	/* Pokud datové bloky nejsou v pořadí */
            fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
        }
        fread(block_buffer, sizeof(block_buffer), 1, fs);
        fwrite(block_buffer, sizeof(block_buffer), 1, f);
        fflush(f);
        prev = blocks[i];
    }

    memset(block_buffer, 0, CLUSTER_SIZE);
    if (rest != 0)
        tmp = rest;
    else
        tmp = CLUSTER_SIZE;

    fseek(fs, sb->data_start_address + blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
    fread(block_buffer, tmp, 1, fs);
    fwrite(block_buffer, tmp, 1, f);

    fflush(fs);
    fclose(f);
    free(blocks);

    printf(OK);
}



/* Vypíše informace o souboru / adresáři
* cesta param ... název souboru / adresáře (+ cesta)
*/
void info(char *path){
    directory *dir;
    char *name;
    directory_item *item;

    if (!fs_formatted){
        print_format_msg();
        return;
    }

    if (!path || path == ""){
        printf(FILENF);
        return;
    }

    /* Parsuje cestu ke zdroji + najde zdrojový adresář */
    if (parse_path(path, &name, &dir)){
        printf(FILENF);
        return;
    }

    /* Pokud je adresář kořen */
    if (dir == directories[0] && strlen(name) == 0){
        print_info(dir->current);
        return;
    }

    /* Hledání položky mezi soubory */
    if (item = find_item(dir->file, name)){
        print_info(item);
        return;

    }

    /* Hledání položky mezi podadresáři */
    if (item = find_item(dir->subdir, name)){
        print_info(item);
        return;
    }

    printf(FILENF);
}


/* Uloží soubor s příkazy k provedení
 * soubor param ... název souboru s příkazy (+ cesta), vrátí otevřený soubor
 */
FILE *load(char *file){
    FILE *f;

    if (!fs_formatted){
        print_format_msg();
        return NULL;
    }

    if ((f = fopen(file, "r")) == NULL){
        printf(FILENF);
        return NULL;
    }
    file_input = 1;	/* Nastaví indikátor, který načte příkazy ze souboru */

    printf(OK);
    return f;
}



/* Ověří zadané velikosti souborového systému a převést jej na bajty
 * velikost parametru ... velikost souborového systému jako řetězce
 * vrátí ... velikost souborového systému v bajtech
 */
int32_t get_size(char *size){
    char *units = NULL;
    long number;

    if (!size || size == ""){
        printf(CCF);
        return ERROR;
    }

    number = strtol(size, &units, 0);

    if (number == 0 && errno != 0){
        printf(CCF);
        return ERROR;
    }

    if (strncmp("KB", units, 2) == 0){			/* Kilobajty */
        number *= 1000;
    }
    else if (strncmp("MB", units, 2) == 0){	    /* Megabajty */
        number *= 1000000;
    }
    else if (strncmp("GB", units, 2) == 0){	    /* Gigabajty */
        number *= 1000000000;
    }

    if (number < MIN_FS_SIZE){			/* Jestli velikost není dostatečně velká */
        printf(CCF);
        return ERROR;
    }
    else if (number > INT_MAX){	        /* Pokud je velikost příliš velká */
        printf(CCF);
        return ERROR;
    }

    return (int32_t)number;
}


/* Najde bezplatný i-uzel
 * vrátí ... ID i-uzlu nebo -1, pokud není žádný i-uzel volný
 */
int32_t find_free_inode(){
    int i;
    /* Hledání bezplatného i-uzlu */
    for (i = 1; i < sb->inode_count; i++){
        if (inodes[i].nodeid == FREE){
            return i;
        }
    }
    return ERROR;
}


/* Najde volné datové bloky v bitmapě
 * počet param ... počet datových bloků
 * vraťí pole volných datových bloků nebo NULL, pokud nebyl nalezen dostatek bloků
 */
int32_t *find_free_data_blocks(int count){
    int i, j = 0;
    int32_t *blocks = (int32_t *)malloc(sizeof(int32_t) * count);

    /* Pokusí se najít po sobě jdoucí bloky */
    for (i = 1; i < sb->data_cluster_count; i++){
        if (bitmap[i] == 0) {
            if ((j != 0) && (blocks[j - 1] != (i - 1))){
                j = 0;
                i--;
                continue;
            }
            blocks[j++] = i;
            if (j == count){
                return blocks;
            }
        }
    }

    j = 0;
    /* Pokud bloky nenasledují za sebou */
    for (i = 1; i < sb->data_cluster_count; i++){
        if (bitmap[i] == 0) {
            blocks[j] = i;
            j++;
            if (j == count)
                return blocks;
        }
    }
    free(blocks);
    return NULL;
}


/* Získá čísla všech datových bloků konkrétní položky
 * položka param ... položka, ze které získáváme datové bloky
 * param block_count ... adresa pro uložení počtu datových bloků
 * param rest ... adresa pro uložení zbytkové velikosti posledního datového bloku (pouze se souborem)
 * vrací pole čísel datových bloků
 */
int32_t *get_data_blocks(int32_t nodeid, int *block_count, int *rest){
    int32_t *blocks, number;
    int i, tmp, counter;
    int max_numbers = 4136;
    inode *node = &inodes[nodeid];

    if (node->isDirectory){
        counter = 0;
        blocks = (int32_t *)malloc(sizeof(int32_t) * max_numbers);

        if (node->direct1 != FREE){
            blocks[counter++] = node->direct1;
        }
        if (node->direct2 != FREE){
            blocks[counter++] = node->direct2;
        }
        if (node->direct3 != FREE){
            blocks[counter++] = node->direct3;
        }
        if (node->direct4 != FREE){
            blocks[counter++] = node->direct4;
        }
        if (node->direct5 != FREE){
            blocks[counter++] = node->direct5;
        }
        if (node->indirect1 != FREE){
            fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
            for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++){
                fread(&number, sizeof(int32_t), 1, fs);
                if (number > 0) {
                    blocks[counter++] = number;
                }
            }
        }
        if (node->indirect2 != FREE){
            fseek(fs, sb->data_start_address + node->indirect2 * CLUSTER_SIZE, SEEK_SET);
            for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++){
                fread(&number, sizeof(int32_t), 1, fs);
                if (number > 0){
                    blocks[counter++] = number;
                }
            }
        }
        *block_count = counter;
    }
    else{	/* Pokud je položka soubor */
        *block_count = node->file_size / CLUSTER_SIZE;
        *rest = node->file_size % CLUSTER_SIZE;
        if (*rest != 0){
            (*block_count)++;
        }

        blocks = (int32_t *)malloc(sizeof(int32_t) * (*block_count));

        blocks[0] = node->direct1;
        if (*block_count > 1){
            blocks[1] = node->direct2;
            if (*block_count > 2){
                blocks[2] = node->direct3;
                if (*block_count > 3){
                    blocks[3] = node->direct4;
                    if (*block_count > 4){
                        blocks[4] = node->direct5;
                        if (*block_count > 5){
                            if (*block_count > 261){	/* Jsou použity oba nepřímé odkazy */
                                /* Přečte všechny datové bloky nepřímého1 */
                                fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
                                fread(&blocks[5], sizeof(int32_t), MAX_NUMBERS_IN_BLOCK, fs);

                                tmp = *block_count - 261;
                                fseek(fs, sb->data_start_address + node->indirect2 * CLUSTER_SIZE, SEEK_SET);
                                fread(&blocks[261], sizeof(int32_t), tmp, fs);
                            }
                            else {	/* Použije se pouze první nepřímý odkaz */
                                tmp = *block_count - 5;
                                fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
                                fread(&blocks[5], sizeof(int32_t), tmp, fs);
                            }
                        }
                    }
                }
            }
        }
    }
    fflush(fs);
    return blocks;
}

/* Najde konkrétní položku se specifickým názvem v seznamu položek počínaje first_item
 * param first_item ... první položka v seznamu položek (adresáře nebo soubory)
 * název parametru ... název nalezené položky, vrátí založenou položku nebo NULL
*/
directory_item *find_item(directory_item *first_item, char *name){
    directory_item *item = first_item;

    while (item != NULL){
        if (strcmp(name, item->item_name) == 0){
            return item;
        }
        item = item->next;
    }
    return NULL;
}


/* Parsuje (Rozděluje) cestu k souboru / adresáři
 * cesta param ... cesta + název souboru / adresáře
 * název parametru ... adresa pro uložení názvu souboru / adresáře
 * adresář param ... adresa pro uložení založeného adresáře
 * vrátí 0 = adresář nalezen, -1 = nenalezen
*/
int parse_path(char *path, char **name, directory **dir){
    int length;
    char buff[BUFF_SIZE];


    if (!path || path == ""){
        return ERROR;
    }

    if ((*name = strrchr(path, '/')) == NULL){	/* Pokud cesta obsahuje pouze název adresáře / souboru */
        *name = path;
        *dir = working_directory;
    }
    else{
        /* Oddělí název adresáře / souboru od zbytku cesty */
        length = strlen(path) - strlen(*name);
        if (path[0] == '/') {
            if (!strchr(path + 1, '/'))	/* Pokud cesta obsahuje pouze kořenový adresář (a název vytvářejícího adresáře) */
                length = 1;
        }

        *name = *name + 1;
        memset(buff, '\0', BUFF_SIZE);
        strncpy(buff, path, length);

        /* Najděte adresář */
        *dir = find_directory(buff, NULL);

        if (!(*dir)){
            return ERROR;
        }
    }

    return NO_ERROR;
}

/* Vytvoří nový adresář
 * nadřazený nadřazený ... nadřazený adresář
 * název parametru ... název adresáře
 * návrat 0 = žádná chyba, -1 = chyba
 */
int create_directory(directory *parent, char *name){
    int32_t inode_id, *data_block;
    directory_item **temp;


    inode_id = find_free_inode();
    if (inode_id == ERROR){
        return ERROR;
    }

    /* Získá ID bezplatného i-uzlu */
    data_block = find_free_data_blocks(1);
    if (data_block == NULL){
        return ERROR;
    }

    /* Vytvoří adresář */
    directory *newdir = (directory *)malloc(sizeof(directory));
    newdir->parent = parent;
    newdir->current = create_directory_item(inode_id, name);
    newdir->file = NULL;
    newdir->subdir = NULL;

    directories[inode_id] = newdir;
    bitmap[data_block[0]] = 1;

    /* Inicializuje i-uzel nového adresáře */
    inodes[inode_id].nodeid = inode_id;
    inodes[inode_id].isDirectory = 1;
    inodes[inode_id].references = 1;
    inodes[inode_id].file_size = 0;
    inodes[inode_id].direct1 = data_block[0];


    temp = &(parent->subdir);
    while (*temp != NULL){
        temp = &((*temp)->next);
    }

    *temp = directories[inode_id]->current;

    if (update_directory(parent, newdir->current, 1)){
        return ERROR;
    }

    update_inode(inode_id);
    update_bitmap(newdir->current, 1, data_block, 1);
    free(data_block);

    return NO_ERROR;
}

/* Vytvoří novou položku adresáře
 * param inode_id ... ID i-uzlu
 * název parametru ... název souboru / adresáře
 */
directory_item *create_directory_item(int32_t inode_id, char *name){
    char buff[12] = {'\0'};

    directory_item *dir_item = (directory_item *)malloc(sizeof(directory_item));

    strncpy(buff, name, strlen(name));
    dir_item->inode = inode_id;
    strncpy(dir_item->item_name, buff, 12);
    dir_item->next = NULL;

    return dir_item;
}


/* Testík, zda adresář již obsahuje položku se stejným názvem
 * adresář param ... adresář
 * název parametru ... název testované položky
 * návrat 1 = existuje, 0 = neexistuje
 */
int test_existence(directory *dir, char *name){
    directory_item *item;
    item = dir->file;
    while (item != NULL){
        if (strcmp(name, item->item_name) == 0){
            return 1;
        }
        item = item->next;
    }

    /* Pokud již existuje adresář se stejným názvem */
    item = dir->subdir;
    while (item != NULL){
        if (strcmp(name, item->item_name) == 0 ){
            return 1;
        }
        item = item->next;
    }
    return 0;
}

/*
 * Funkce pro vyhledání posledního indexu libovolného znaku v daném řetězci
 */
int lastIndexOf(const char * str, const char toFind){
    int index = -1;
    int i = 0;

    while(str[i] != '\0'){
        // Update index if match is found
        if(str[i] == toFind){
            index = i;
        }
        i++;
    }

    return index;
}

/* Najde zadaný adresář podle cesty
 * návrat ... adresář nebo NULL, pokud nebyl nalezen
 */
directory *find_directory(char *path, char **name){
    int found, path_len = 0, last_occurence = 0;
    char *part;	                            /* Část cesty */
    char *delim = "/";
    directory *dir;
    directory_item *item;

    path_len = strlen(path);
    last_occurence = lastIndexOf(path, '/');    /* najde index oddělovače posledního výskytu */

    if (path[0] == '/'){	                /* Absolutní */
        dir = directories[0];
    }
    else{					                /* Relativní */
        dir = working_directory;
    }

    part = strtok(path, delim);

    while(part != NULL){

        if (strcmp(part, ".") == 0){
            part = strtok(NULL, delim);
            continue;
        }
        else if (strcmp(part, "..") == 0){	/* Přejde do nadřazeného adresáře */
            dir = dir->parent;
            part = strtok(NULL, delim);
            continue;
        }
        else if (strlen(part) == (path_len-last_occurence-1) && name != NULL){
            *name = part;
              if (test_existence(dir,*name)==1){
                    dir = directories[dir->subdir->inode];
              }
              break;
        }
        else{
            found = 0;
            item = dir->subdir;
            while (item != NULL){
                if (strcmp(part, directories[item->inode]->current->item_name) == 0){
                    dir = directories[item->inode];
                    part = strtok(NULL, delim);

                    found = 1;
                    break;
                }
                item = item->next;
            }

            if (found == 0){	/* Takový adresář nebyl nalezen */
                return NULL;
            }
        }
    }
    return dir;
}

/*	Určé konzistenci */
void check(){
    int i, i_count;
    float f_count;
    int tmp, block_count, rest, prev;
    char *name;
    int pom = 0; /* pomocna true false */
    directory_item *item;
    int32_t *blocks;
    directory *dir;

    for (i = 0; i < sb->inode_count; i++){
        if (inodes[i].isDirectory == 0 && inodes[i].nodeid !=-1){
            blocks = get_data_blocks(inodes[i].nodeid, &block_count, &rest);
            i_count = (int)(inodes[i].file_size / CLUSTER_SIZE);
            f_count = (float)inodes[i].file_size / CLUSTER_SIZE;
            if (((float)i_count - f_count) < 0.f) {
                i_count += 1;
            }
        }

        if (inodes[i].isDirectory == 0 && inodes[i].nodeid !=-1){
            if (inodes[i].references == 0){
                printf("Wrong consistency \n");;
                pom = 1;
                return;
            }
        }
    }

    if(pom == 1){
        printf("Wrong consistency \n");;
    }else{
        printf("Consistency OK \n");
    }

}

/* Má za úkol vytvořit špatnou konzistenci */
void dis(){
    inodes->references = 0;
    inodes[0].references = 0;
    inodes[1].references = 0;
    inodes[2].references = 0;
    inodes[4].references = 0;
    inodes[5].references = 0;
    check();
}


/* Volná přidělená paměť pro adresáře */
void free_directories(directory *root){
    directory_item *f, *d, *t;

    if (!root) return;

    d = root->subdir;
    while (d != NULL){
        free_directories(directories[d->inode]);
        d = d->next;
    }

    f = root->file;
    while (f != NULL){
        t = f->next;
        free(f);
        f = t;
    }

    free(d);
    d = NULL;
    free(root->current);
    free(root);
    root = NULL;
}

/* Nastaví konkrétní i-uzel jako volný
 * ID parametru ... ID i-uzlu
 */
void clear_inode(int id){
    inodes[id].nodeid = FREE;
    inodes[id].isDirectory = 0;
    inodes[id].references = 0;
    inodes[id].file_size = 0;
    inodes[id].direct1 = FREE;
    inodes[id].direct2 = FREE;
    inodes[id].direct3 = FREE;
    inodes[id].direct4 = FREE;
    inodes[id].direct5 = FREE;
    inodes[id].indirect1 = FREE;
    inodes[id].indirect2 = FREE;
}


/* Aktualizace velikosti všech i-uzlů na cestě z adresáře do root
 * param dir ... adresář, ze kterého se v hierarchii pohybujeme nahoru
 * velikost parametru ... přidání velikosti k původní velikosti (může být záporná, pokud odeberete soubor)
 */
void update_sizes(directory *dir, int32_t size){
    directory *d = dir;
    while (d != directories[0]) {
        inodes[d->current->inode].file_size += size;
        update_inode(d->current->inode);
        d = d->parent;
    }

    inodes[d->current->inode].file_size += size;
    update_inode(d->current->inode);
}


/* Tisk informací o souboru / adresáři
 * položka param ... soubor nebo adresář
 */
void print_info(directory_item *item){
    int i;
    int32_t number;
    inode node = inodes[item->inode];

    printf("%s - %dB - i-node %d -", item->item_name, node.file_size, node.nodeid);
    printf(" Dir:");
    if (node.direct1 != FREE){
        printf(" %d", node.direct1);
    }
    if (node.direct2 != FREE){
        printf(" %d", node.direct2);
    }
    if (node.direct3 != FREE){
        printf(" %d", node.direct3);
    }
    if (node.direct4 != FREE){
        printf(" %d", node.direct4);
    }
    if (node.direct5 != FREE){
        printf(" %d", node.direct5);
    }
    printf(" Indir:");
    if (node.indirect1 != FREE){
        printf(" (%d)", node.indirect1);
        fseek(fs, sb->data_start_address + node.indirect1 * CLUSTER_SIZE, SEEK_SET);
        for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++){
            fread(&number, sizeof(int32_t), 1, fs);
            if (number == 0){
                break;
            }
            printf(" %d", number);
        }
    }
    if (node.indirect2 != FREE){
        printf(" (%d)", node.indirect2);
        fseek(fs, sb->data_start_address + node.indirect2 * CLUSTER_SIZE, SEEK_SET);
        for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++){
            fread(&number, sizeof(int32_t), 1, fs);
            if (number == 0){
                break;
            }
            printf(" %d", number);
        }
    }
    printf("\n");
}


/* Vytisknout obsah souboru
 * položka param ... tisk souboru
 */
void print_file(directory_item *item){
    int i, tmp, block_count, rest, prev;
    int32_t *blocks;

    blocks = get_data_blocks(item->inode, &block_count, &rest);       /* Získejte datové bloky souboru */

    prev = blocks[0];
    for (i = 0; i < block_count - 1; i++){
        if (prev != blocks[i] - 1) {
            fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
        }
        fread(block_buffer, CLUSTER_SIZE, 1, fs);
        printf("%s", block_buffer);
        prev = blocks[i];
    }
    memset(block_buffer, 0, CLUSTER_SIZE);
    if (rest != 0)
        tmp = rest;
    else
        tmp = CLUSTER_SIZE;

    fseek(fs, sb->data_start_address + blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
    fread(block_buffer, tmp, 1, fs);
    printf("%s", block_buffer);

    fflush(fs);
    free(blocks);
}


/* Tisk zprávy neformátovaného souborového systému. */
void print_format_msg(){
    printf(" The filesystem has to be formatted first. \nUsage: format [size] \n");
}

/* Nastaví i-uzel jako soubor a inicializovat všechny datové bloky
 * ID parametru ... ID i-uzlu
 * velikost parametru ... velikost souboru
 * param block_count ... počet datových bloků obsazených souborem
 * param tmp_count ... block_count + bloky pro nepřímé odkazy
 * param last_block_index ... adresa indexu k poslednímu datovému bloku souboru
 * param bloky ... datové bloky
 */
void initialize_inode(int32_t id, int32_t size, int block_count, int tmp_count, int *last_block_index, int32_t *blocks){
    int tmp;
    inode *node = &inodes[id];

    node->nodeid = id;
    node->isDirectory = 0;
    node->references = 1;
    node->file_size = size;
    node->direct1 = blocks[0];

    *last_block_index = 0;
    if (block_count > 1){
        node->direct2 = blocks[1];
        *last_block_index = 1;

        if (block_count > 2){
            node->direct3 = blocks[2];
            *last_block_index = 2;

            if (block_count > 3){
                node->direct4 = blocks[3];
                *last_block_index = 3;

                if (block_count > 4){
                    node->direct5 = blocks[4];
                    *last_block_index = 4;

                    if (block_count > 5){
                        node->indirect1 = blocks[tmp_count - 1];


                        if (block_count > 261){
                            node->indirect2 = blocks[tmp_count - 2];
                            *last_block_index = tmp_count - 3;
                            fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
                            fwrite(&blocks[5], sizeof(int32_t), MAX_NUMBERS_IN_BLOCK, fs);

                            tmp = block_count - 261;
                            fseek(fs, sb->data_start_address + node->indirect2 * CLUSTER_SIZE, SEEK_SET);
                            fwrite(&blocks[261], sizeof(int32_t), tmp, fs);
                        }
                        else{
                            *last_block_index = tmp_count - 2;
                            tmp = block_count - 5;
                            fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
                            fwrite(&blocks[5], sizeof(int32_t), tmp, fs);
                        }
                    }
                }
            }
        }
    }
}


/*
 * Načtěte souborový systém ze souboru
 */
void load_fs(){
    directory *root;
    int i;

    if (!fs){
        fs = fopen(fs_name, "rb+");
    }

    /* uložé superblock */
    sb = (superblock *)malloc(sizeof(superblock));
    if (!sb){
        printf("Filesystem loading failed.\n");
        return;
    }

    fread(&(sb->disk_size), sizeof(int32_t), 1, fs);
    fread(&(sb->cluster_size), sizeof(int32_t), 1, fs);
    fread(&(sb->cluster_count), sizeof(int32_t), 1, fs);
    fread(&(sb->inode_count), sizeof(int32_t), 1, fs);
    fread(&(sb->bitmap_cluster_count), sizeof(int32_t), 1, fs);
    fread(&(sb->inode_cluster_count), sizeof(int32_t), 1, fs);
    fread(&(sb->data_cluster_count), sizeof(int32_t), 1, fs);
    fread(&(sb->bitmap_start_address), sizeof(int32_t), 1, fs);
    fread(&(sb->inode_start_address), sizeof(int32_t), 1, fs);
    fread(&(sb->data_start_address), sizeof(int32_t), 1, fs);

    bitmap = (int8_t *)malloc(sb->data_cluster_count);
    inodes = (inode *)malloc(sizeof(inode) * sb->inode_count);
    directories = (directory **)malloc(sizeof(directory *) * sb->inode_count);
    if (!bitmap || !inodes || !directories){
        printf(CCF);
        return;
    }

    /* uložení bitmapy */
    fseek(fs, sb->bitmap_start_address, SEEK_SET);
    fread(bitmap, sizeof(int8_t), sb->data_cluster_count, fs);

    /* uložení i-uzly */
    fseek(fs, sb->inode_start_address, SEEK_SET);
    for (i = 0; i < sb->inode_count; i++){
        fread(&(inodes[i].nodeid), sizeof(int32_t), 1, fs);
        fread(&(inodes[i].isDirectory), sizeof(int8_t), 1, fs);
        fread(&(inodes[i].references), sizeof(int8_t), 1, fs);
        fread(&(inodes[i].file_size), sizeof(int32_t), 1, fs);
        fread(&(inodes[i].direct1), sizeof(int32_t), 1, fs);
        fread(&(inodes[i].direct2), sizeof(int32_t), 1, fs);
        fread(&(inodes[i].direct3), sizeof(int32_t), 1, fs);
        fread(&(inodes[i].direct4), sizeof(int32_t), 1, fs);
        fread(&(inodes[i].direct5), sizeof(int32_t), 1, fs);
        fread(&(inodes[i].indirect1), sizeof(int32_t), 1, fs);
        fread(&(inodes[i].indirect2), sizeof(int32_t), 1, fs);
    }

    root = (directory *)malloc(sizeof(directory));
    if (!root){
        printf(CCF);
        return;
    }
    root->current = create_directory_item(0,"/");
    root->parent = root;
    root->subdir = NULL;
    root->file = NULL;

    working_directory = root;	/* Nastavit root jako aktuální adresář */
    directories[0] = root;
    load_directory(root, 0);
}


/* Načte všechny položky adresáře ze souboru
 * adresář param ... adresář
 * param id      ... id adresáře
 */
void load_directory(directory *dir, int id){
    int i, j, block_count, rest;
    int inode_count = 64;	        /* Maximální počet i-uzlů v jednom datovém bloku */
    int32_t *blocks;		        /* Počet datových bloků */
    int32_t nodeid;			        /* ID položky */
    char name[12];			        /* Název položky */
    directory *newdir;
    directory_item *item, *temp;
    directory_item **psubdir = &(dir->subdir);	/* Adresa posledního (volného) podadresáře v seznamu podadresářů */
    directory_item **pfile = &(dir->file);		/* Adresa posledního (volného) souboru v seznamu souborů */

    blocks = get_data_blocks(dir->current->inode, &block_count, NULL);

    for (i = 0; i < block_count; i++){		        /* Iterace přes datové bloky */
        fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
        for (j = 0; j < inode_count; j++){	        /* Iterace nad položkami v datovém bloku */
            fread(&nodeid, sizeof(int32_t), 1, fs);		/* Přečíst ID inode, pokud id <1 -> neplatná položka a přeskočit na další položku */
            if (nodeid > 0) {
                fread(name, sizeof(name), 1, fs);
                item = create_directory_item(nodeid, name);
                if (inodes[nodeid].isDirectory){	 /* Pokud je položka adresář */
                    *psubdir = item;
                    psubdir = &(item->next);
                }
                else{
                    *pfile = item;
                    pfile = &(item->next);
                }
            }
            else{
                fseek(fs, sizeof(name), SEEK_CUR);	/* Přeskočit místo názvu souboru / adresáře */
            }
        }
    }
    free(blocks);

    /* Rekurzivní volání funkce na všechny načtené podadresáře */
    temp = dir->subdir;
    while (temp != NULL){
        newdir = (directory *)malloc(sizeof(directory));
        newdir->parent = dir;
        newdir->current = temp;
        newdir->subdir = NULL;
        newdir->file = NULL;

        directories[temp->inode] = newdir;
        load_directory(newdir, temp->inode);

        temp = temp->next;
    }
}


/* Aktualizuje bitmapu v souboru podle souboru / adresáře
 * položka param  ... soubor / adresář
 * hodnota parametru  ... 1 = použití datových bloků, 0 = volné datové bloky
 * param data_blocks  ... datové bloky položky nebo NULL (adresář)
 * param b_count  ... počet bloků, pokud není NULL
 */
void update_bitmap(directory_item *item, int8_t value, int32_t *data_blocks, int b_count){
    int i, block_count, rest;
    int32_t *blocks;

    if (!data_blocks){
        blocks = get_data_blocks(item->inode, &block_count, NULL);
    }
    else{
        blocks = data_blocks;
        block_count = b_count;
    }
    for (i = 0; i < block_count; i++){
        bitmap[blocks[i]] = value;
        fseek(fs, sb->bitmap_start_address + blocks[i], SEEK_SET);
        fwrite(&value, sizeof(int8_t), 1, fs);
    }

    /* Bloky nepřímých odkazů */
    if (inodes[item->inode].indirect1 != FREE){
        bitmap[inodes[item->inode].indirect1] = value;
        fseek(fs, sb->bitmap_start_address + inodes[item->inode].indirect1, SEEK_SET);
        fwrite(&value, sizeof(int8_t), 1, fs);
    }
    if (inodes[item->inode].indirect2 != FREE){
        bitmap[inodes[item->inode].indirect2] = value;
        fseek(fs, sb->bitmap_start_address + inodes[item->inode].indirect2, SEEK_SET);
        fwrite(&value, sizeof(int8_t), 1, fs);
    }

    fflush(fs);
}


/* Aktualizuje konkrétní i-uzel v souboru
 * param id ... i-node id = offset v souboru od začátku i-uzlů
 */
void update_inode(int id){
    fseek(fs, sb->inode_start_address + id * INODE_SIZE, SEEK_SET);

    fwrite(&(inodes[id].nodeid), sizeof(int32_t), 1, fs);
    fwrite(&(inodes[id].isDirectory), sizeof(int8_t), 1, fs);
    fwrite(&(inodes[id].references), sizeof(int8_t), 1, fs);
    fwrite(&(inodes[id].file_size), sizeof(int32_t), 1, fs);
    fwrite(&(inodes[id].direct1), sizeof(int32_t), 1, fs);
    fwrite(&(inodes[id].direct2), sizeof(int32_t), 1, fs);
    fwrite(&(inodes[id].direct3), sizeof(int32_t), 1, fs);
    fwrite(&(inodes[id].direct4), sizeof(int32_t), 1, fs);
    fwrite(&(inodes[id].direct5), sizeof(int32_t), 1, fs);
    fwrite(&(inodes[id].indirect1), sizeof(int32_t), 1, fs);
    fwrite(&(inodes[id].indirect2), sizeof(int32_t), 1, fs);

    fflush(fs);
}


/* Aktualizuje adresář - přidat / odebrat položku ze souboru
 * adresář param ... adresář
 * položka param ... položka přidání / odebrání do / z adresáře
 * akce param ... 1 = přidat položku, 0 = odebrat položku
 * vrátí 0 = úspěch, -1 = všechny datové bloky adresáře jsou plné nebo odstraněná položka nebyla nalezena
 */
int update_directory(directory *dir, directory_item *item, int action){
    int i, j, block_count, prev, item_count, found = 0;
    int32_t *blocks, *free_block;
    int name_length = 12;
    int zeros[4] = {0};  // buffer with zeros - for removing the item from the file
    int max_items_in_block = 64;
    int32_t nodeid;
    inode *dir_node;

    blocks = get_data_blocks(dir->current->inode, &block_count, NULL);

    if (action == 1){
        for (i = 0; i < block_count; i++){
            fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
            for (j = 0; j < max_items_in_block; j++){
                fread(&nodeid, sizeof(int32_t), 1, fs);
                if (nodeid == 0){	            /* Nalezeno volné místo -> uložit položku */
                    fseek(fs, -4, SEEK_CUR);
                    fflush(fs);
                    fwrite(&(item->inode), sizeof(int32_t), 1, fs);
                    fwrite(item->item_name, sizeof(item->item_name), 1, fs);
                    fflush(fs);
                    free(blocks);
                    return NO_ERROR;
                }
                else{
                    fseek(fs, name_length, SEEK_CUR);	/* Přeskočit mezeru pro jméno */
                }
            }
        }

        /* V aktuálních datových blocích adresáře nebylo nalezeno žádné volné místo -> zkuste najít další volný datový blok */
        free_block = find_free_data_blocks(1);	/* Použijte přímý odkaz */
        if (!free_block){
            return ERROR;
        }

        dir_node = &(inodes[dir->current->inode]);

        if (dir_node->direct1 == FREE){
            dir_node->direct1 = free_block[0];
        }
        else if (dir_node->direct2 == FREE){
            dir_node->direct2 = free_block[0];
        }
        else if (dir_node->direct3 == FREE){
            dir_node->direct3 = free_block[0];
        }
        else if (dir_node->direct4 == FREE){
            dir_node->direct4 = free_block[0];
        }
        else if (dir_node->direct5 == FREE){
            dir_node->direct5 = free_block[0];
        }
        else{
            free(free_block);
            free_block = find_free_data_blocks(2);	/* Použije nepřímý odkaz (potřebujete 2 bloky zdarma) */
            if (!free_block)
                return ERROR;

            if (dir_node->indirect1 == FREE){
                dir_node->indirect1 = free_block[1];
                fseek(fs, sb->data_start_address + free_block[1] * CLUSTER_SIZE, SEEK_SET);
                fwrite(&(free_block[0]), sizeof(int32_t), 1, fs);
            }
            else if (dir_node->indirect2 == FREE){
                dir_node->indirect2 = free_block[1];
                fseek(fs, sb->data_start_address + free_block[1] * CLUSTER_SIZE, SEEK_SET);
                fwrite(&(free_block[0]), sizeof(int32_t), 1, fs);
            }
        }

        fseek(fs, sb->data_start_address + free_block[0] * CLUSTER_SIZE, SEEK_SET);
        fwrite(&(item->inode), sizeof(int32_t), 1, fs);
        fwrite(item->item_name, sizeof(item->item_name), 1, fs);

        fflush(fs);
        update_bitmap(dir->current, 1, NULL, 0);
        update_inode(dir->current->inode);
        free(free_block);
        free(blocks);
        return NO_ERROR;
    }
    else{	    /* Odebere položku (najít položku s konkrétním ID) */
        prev = blocks[0];
        for (i = 0; i < block_count; i++){
            if (prev != (blocks[i] - 1)){
                fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
            }

            item_count = 0;	    // Counter of items in this data block
            for (j = 0; j < max_items_in_block; j++){
                fread(&nodeid, sizeof(int32_t), 1, fs);
                if (nodeid > 0)
                    item_count++;

                if (!found){
                    if (nodeid == (item->inode)){
                        fseek(fs, -4, SEEK_CUR);
                        fflush(fs);
                        fwrite(&zeros, sizeof(zeros), 1, fs);
                        fflush(fs);
                        found = 1;
                        if (item_count > 1)
                            break;
                    }
                }
            }
            if (found){	            /* Pokud jedinou položkou v datovém bloku bylo odebírání položky -> volný datový blok */
                if (item_count == 1){
                    remove_reference(dir->current, i);
                }
                free(blocks);
                return NO_ERROR;
            }
            prev = blocks[i];
        }
    }
    return ERROR;
}


/* Odebere odkaz na datový blok z i-uzlu (kromě datového bloku direct1) (+ ze souboru s nepřímými odkazy)
 * položka param   ... adresář, ve kterém odstraníme datový blok
 * param block_id  ... číslo odstraněného datového bloku
 */
void remove_reference(directory_item *item, int32_t block_id){
    int i, j;
    int32_t number, count, found, zero = 0, blocks[2];
    inode *node = &inodes[item->inode];

    if (node->direct1 == block_id){	    /* První přímý blok se neodstraní */
        return;
    }
    else if (node->direct2 == block_id){
        blocks[0] = node->direct2;
        node->direct2 = FREE;
    }
    else if (node->direct3 == block_id){
        blocks[0] = node->direct3;
        node->direct3 = FREE;
    }
    else if (node->direct4 == block_id){
        blocks[0] = node->direct4;
        node->direct4 = FREE;
    }
    else if (node->direct5 == block_id){
        blocks[0] = node->direct5;
        node->direct5 = FREE;
    }
    else{
        for (i = 0; i < 2; i++){
            if (i == 0)	        /* Procházet nepřímým1 */
                fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
            else 		        /* Procházet nepřímým2 */
                fseek(fs, sb->data_start_address + node->indirect2 * CLUSTER_SIZE, SEEK_SET);

            count = 0;
            found = 0;
            for (j = 0; j < MAX_NUMBERS_IN_BLOCK; j++){
                fread(&number, sizeof(int32_t), 1, fs);
                if (number > 0)
                    count++;
                if (!found){
                    if (number == block_id){
                        found = 1;
                        blocks[0] = number;
                        fseek(fs, -4, SEEK_CUR);
                        fflush(fs);
                        fwrite(&zero, sizeof(int32_t), 1, fs);
                        fflush(fs);
                        if (count > 1)
                            break;
                    }
                }
            }

            if (found){
                if (count == 1){
                    if (i == 0){
                        blocks[1] = node->indirect1;
                        node->indirect1 = FREE;
                    }
                    else{
                        blocks[1] = node->indirect2;
                        node->indirect2 = FREE;
                    }
                }
                break;
            }
        }
    }
    update_bitmap(item, 0, blocks, 1);
    update_inode(item->inode);
}

