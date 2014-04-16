#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <fitsio.h>
#include "sep.h"

uint64_t gettime_ns()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000ULL;
}

void write_region_file(objliststruct *catalog, char *fname)
{

}

int main(int argc, char **argv)
{
  char fname[] = "DECam_00179121_24.fits[0]";
  float *im;
  fitsfile *file;
  int status=0;
  int npix;
  uint64_t t0, t1;
  long fpixel[2] = {1, 1};
  long naxes[2];
  int *anynull=0;
  int i;

  /* Get filename
  if (argc != 2)
    {
      puts("Usage: runtest FILENAME");
      exit(1);
    }
  fname = argv[1];
  */

  /* Read in image */
  puts(fname);
  fits_open_file(&file, fname, READONLY, &status); 
  puts("opened file");
  fits_get_img_size(file, 2, naxes, &status);
  npix = naxes[0]*naxes[1];
  printf("image size = %ld, %ld\n", naxes[0], naxes[1]);
  im = (float*)malloc(npix*sizeof(float));
  ffgpxv(file, TFLOAT, fpixel, npix, 0, im, anynull, &status);
  fits_close_file(file, &status);
  if (status)
    goto exit;

  printf("Running makeback... ");
  backmap *bkmap;
  t0 = gettime_ns();
  status = makeback(im, NULL, naxes[0], naxes[1], 64, 64, 0.0, 3, 3, 0.0,
		    &bkmap);
  if (status)
    goto exit;
  t1 = gettime_ns();
  printf("done in %.1f ms.\n", (double)(t1 - t0)/1000000.);

  /* skip this for now */
  if (1)
    {
      /* write background image out */
      float *bkim = (float*)malloc(npix*sizeof(float));
      printf("evaluting background map...");
      t0 = gettime_ns();
      status = backrmsarray(bkmap, bkim);
      if (status)
	goto exit;
      t1 = gettime_ns();
      printf("done in %.1f ms.\n", (double)(t1 - t0)/1000000.);

      printf("writing to file: sepbackrms.fits...");
      fitsfile *f;
      ffinit(&f, "!sepbackrms.fits", &status); /* open new image */
      ffcrim(f, FLOAT_IMG, 2, naxes, &status); /* create image extension */
      ffppx(f, TFLOAT, fpixel, npix, bkim, &status); 
      fits_close_file(f, &status);
      free(bkim);
      bkim = NULL;
    }

  /* subtract background */
  status = subbackarray(bkmap, im);

  /* background variance */
  float *bkgvar = (float*)malloc(npix*sizeof(float));
  status = backvararray(bkmap, bkgvar);

  /* find objects */
  float conv[] = {1,2,1, 2,4,2, 1,2,1};
  objliststruct *catalog = NULL;

  printf("extracting...");
  t0 = gettime_ns();
  float thresh = 1.5 * bkmap->backsig;
  status = extract(im, NULL, naxes[0], naxes[1], thresh, thresh, 0, 5,
		   conv, 3, 3, 32, 0.005, 0, 1.0, &catalog);
  if (status)
    goto exit;
  t1 = gettime_ns();
  printf("done in %.1f ms.\n", (double)(t1 - t0)/1000000.);
  printf("%d objects\n", catalog->nobj);

  /* create a ds9 region file*/
  FILE *fp = fopen("catalog.reg", "w");
  objstruct *obj;
  for (i=0; i<catalog->nobj; i++)
    {
      obj = &catalog->obj[i];
      fprintf(fp, "circle %f %f %f\n",
	      obj->mx+1, obj->my+1, 3.0);
    }
  fclose(fp);

  freeback(bkmap);
 exit:
  printf("status: %d\n", status);
  puts(errdetail);
  return status;
}