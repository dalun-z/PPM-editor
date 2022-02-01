#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>

#define THREADS 1

#define filterWidth 3
#define filterHeight 3

#define RGB_MAX 255

typedef struct {
	 unsigned char r, g, b;
} PPMPixel;


struct parameter {
	PPMPixel *image;         //original image
	PPMPixel *result;        //filtered image
	unsigned long int w;     //width of image
	unsigned long int h;     //height of image
	unsigned long int start; //starting point of work
	unsigned long int size;  //equal share of work (almost equal if odd)
};


/*This is the thread function. It will compute the new values for the region of image specified in params (start to start+size) using convolution.
    (1) For each pixel in the input image, the filter is conceptually placed on top ofthe image with its origin lying on that pixel.
    (2) The  values  of  each  input  image  pixel  under  the  mask  are  multiplied  by the corresponding filter values.
    (3) The results are summed together to yield a single output value that is placed in the output image at the location of the pixel being processed on the input.
 
 */
void *threadfn(void *params)
{
    //PPMPixel *image;
    //PPMPixel *result;

    //struct parameter imgFIle;

    struct parameter *par;
    par = (struct parameter*) params;
	
	int laplacian[filterWidth][filterHeight] =
	{
	  -1, -1, -1,
	  -1,  8, -1,
	  -1, -1, -1,
	};

    int red=0, green=0, blue=0;
    int x_coordinate, y_coordinate;

    /*For all pixels in the work region of image (from start to start+size)
      Multiply every value of the filter with corresponding image pixel. Note: this is NOT matrix multiplication.
      Store the new values of r,g,b in p->result.
     */
    
    for( int iImgHeight = par->start; iImgHeight < (par->start + par->size); iImgHeight++){
        for( int iImgWidth = 0; iImgWidth < par->w; iImgWidth++){
            for( int iFtrWidth = 0; iFtrWidth < filterWidth; iFtrWidth++){
                for( int iFtrHeight = 0; iFtrHeight < filterHeight; iFtrHeight++){
                    x_coordinate = (iImgWidth - filterWidth/2 + iFtrWidth + par->w) % par->w;
                    y_coordinate = (iImgHeight-filterHeight/2 + iFtrHeight+ par->h) % par->h;
                    red += par->image[y_coordinate * par->w + x_coordinate].r * laplacian[iFtrWidth][iFtrHeight];
                    green += par->image[y_coordinate * par->w + x_coordinate].g * laplacian[iFtrWidth][iFtrHeight];
                    blue += par->image[y_coordinate * par->w + x_coordinate].b * laplacian[iFtrWidth][iFtrHeight];
                }
            }
            if(red > 255){
                red = 255;
            }else if(red < 0){
                red = 0;
            }

            if(blue > 255){
                blue = 255;
            }else if(blue < 0){
                blue = 0;
            }

            if(green > 255){
                green = 255;
            }else if(green < 0){
                green = 0;
            }

            par->result[iImgHeight * par->w + iImgWidth].r = red;
            par->result[iImgHeight * par->w + iImgWidth].g = green;
            par->result[iImgHeight * par->w + iImgWidth].b = blue;
            // reset values
            red = 0;
            green = 0;
            blue = 0;
        }
    }
    
		
	return NULL;
}


/*Create a new P6 file to save the filtered image in. Write the header block
 e.g. P6
      Width Height
      Max color value
 then write the image data.
 The name of the new file shall be "name" (the second argument).
 */
void writeImage(PPMPixel *image, char *name, unsigned long int width, unsigned long int height)
{
    FILE *fp;

    fp = fopen(name, "wb");

    fprintf(fp, "P6\n");
    fprintf(fp, "# Created by Dalun Zhang\n");
    fprintf(fp, "%ld %ld\n", width, height);
    fprintf(fp, "%d\n", RGB_MAX);

    fwrite(image, 3 * width, height, fp);

    fclose(fp);
    
}

/* Open the filename image for reading, and parse it.
    Example of a ppm header:    //http://netpbm.sourceforge.net/doc/ppm.html
    P6                  -- image format
    # comment           -- comment lines begin with
    ## another comment  -- any number of comment lines
    200 300             -- image width & height
    255                 -- max color value
 
 Check if the image format is P6. If not, print invalid format error message.
 Read the image size information and store them in width and height.
 Check the rgb component, if not 255, display error message.
 Return: pointer to PPMPixel that has the pixel data of the input image (filename)
 */
PPMPixel *readImage(const char *filename, unsigned long int *width, unsigned long int *height)
{

	PPMPixel *img;
    FILE *fp;
    char buff[16];
    int rgb_max, comment;
	

	//read image format
    fp = fopen(filename, "rb");
    if(!fp){
        fprintf(stderr, "Unable to open file '%s'\n", filename);
        exit(1);
    }

    if(!fgets(buff, sizeof(buff), fp)){
        perror("Error reading from file");
        exit(1);
    }

	//check the image format by reading the first two characters in filename and compare them to P6.
    if(buff[0] != 'P' || buff[1] != '6'){
        fprintf(stderr, "Invaild image format\n");
        exit(1);
    }

    img = (PPMPixel*)malloc(sizeof(PPMPixel));
    if (!img) {
         fprintf(stderr, "Unable to allocate memory\n");
         exit(1);
    }

	//If there are comments in the file, skip them. You may assume that comments exist only in the header block.
    comment = getc(fp);
    while(comment == '#'){
        while(getc(fp) != '\n') 
        ;
            comment = getc(fp);
    }
    ungetc(comment, fp);
	
    //Read file width and height
    fscanf(fp, "%lu", width);
    fscanf(fp, "%lu", height);

	//Read rgb component. Check if it is equal to RGB_MAX. If  not, display error message.
	fscanf(fp, "%d", &rgb_max);
    if(rgb_max != RGB_MAX){
        printf("Invalid rgb component %d", rgb_max);
        exit(1);
    }
    
    while (fgetc(fp) != '\n') ;
    //allocate memory for img. NOTE: A ppm image of w=200 and h=300 will contain 60000 triplets (i.e. for r,g,b), ---> 180000 bytes.
    img = (PPMPixel*)malloc(*width * *height * sizeof(PPMPixel));

    if(!img){
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    fread(img, 3 * *width, *height, fp);

    fclose(fp);

	return img;
}

/* Create threads and apply filter to image.
 Each thread shall do an equal share of the work, i.e. work=height/number of threads.
 Compute the elapsed time and store it in *elapsedTime (Read about gettimeofday).
 Return: result (filtered image)
 */
PPMPixel *apply_filters(PPMPixel *image, unsigned long w, unsigned long h, double *elapsedTime) {

    PPMPixel *result;
    //allocate memory for result
    result = (PPMPixel*)malloc(w * h * sizeof(PPMPixel));

    //allocate memory for parameters (one for each thread)
    struct parameter *par = malloc(sizeof(struct parameter));

    unsigned long int work = h/THREADS;
    pthread_t thread[THREADS];
    par->start = 0;
    par->size = work;
    par->image = image;
    par->result = result;
    par->h = h;
    par->w = w;

    /*create threads and apply filter.
     For each thread, compute where to start its work.  Determine the size of the work. If the size is not even, the last thread shall take the rest of the work.
     */
    for( int i = 0; i < THREADS; i++){
        par->start = (par->size)*i;
        pthread_create(&thread[i], NULL, threadfn, (void*)par);
    //Let threads wait till they all finish their work.
        pthread_join(thread[i], NULL);

    }


	return result;
}


/*The driver of the program. Check for the correct number of arguments. If wrong print the message: "Usage ./a.out filename"
    Read the image that is passed as an argument at runtime. Apply the filter. Print elapsed time in .3 precision (e.g. 0.006 s). Save the result image in a file called laplacian.ppm. Free allocated memory.
 */
int main(int argc, char *argv[])
{
	//load the image into the buffer
    PPMPixel *image;
    PPMPixel *result;
    //struct parameter *p;
    unsigned long int w, h;

    struct timeval time;
    time_t startTime, endTime; 
    double elapsedTime = 0.0;

    // start time
    gettimeofday(&time, NULL);
    startTime = time.tv_usec;

    if(argc == 2){
        image = readImage(argv[1], &w, &h);
        result = apply_filters(image, w, h, &elapsedTime);
        writeImage(result, "laplacian.ppm", w, h);
    }else{
        perror("Usage ./a.out filename");
        exit(0);
    }

    // end time
    gettimeofday(&time, NULL);
    endTime = time.tv_usec;
    elapsedTime = (difftime(endTime, startTime))/1000000;

    printf("%f", elapsedTime);

	return 0;
}
