
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "conversions.h"
#include "mnifile.h"

void read_fruit_salads(MniInfo *MniFile,
                       int     *slices,
                       int     slice_count,
                       int     *frames,
                       int     frame_count)
{
  
  int line, image, base_size, line_size, slice, frame;
  MNIbyte *image_base;
  FruitSaladStructure FruitSalad;
  
  line_size = MniFile->ImageWidth * MniFile->PixelWidth;
  base_size = line_size * FRUIT_SALAD_HEIGHT;
  image_base = (MNIbyte *)malloc(base_size);
  
  for (slice = 0; slice < slice_count; slice++){
    
    for (frame = 0; frame < frame_count; frame++){
      
      image = MniFile->Frames*(slices[slice]-1) + frames[frame]-1;
      
      (void) fseek(MniFile->fp, 
                   MniFile->DataOffset * BLOCK_SIZE + image * MniFile->ImageSize, 
                   SEEK_SET);
      (void) fread(image_base, sizeof(MNIbyte), base_size, MniFile->fp);
      for (line = 0; line < FRUIT_SALAD_HEIGHT; line++){
        (void) memcpy(FruitSalad.Lines[line], 
                      &image_base[line_size * line], 
                      FRUIT_SALAD_WIDTH);
      }
      
      MniFile->ImageParameters[image].Qsc    = convert_r4(FruitSalad.Items.Qsc);
      MniFile->ImageParameters[image].CntImg = convert_r4(FruitSalad.Items.CntImg);
      MniFile->ImageParameters[image].Zpos   = convert_r4(FruitSalad.Items.Zpos);
      MniFile->ImageParameters[image].Izpos  = *(MNIbyte *)FruitSalad.Items.Izpos;
      MniFile->ImageParameters[image].Itype  = *(MNIbyte *)FruitSalad.Items.Itype;
      MniFile->ImageParameters[image].Isc    = *(MNIbyte *)FruitSalad.Items.Isc;
      MniFile->ImageParameters[image].Islice = *(MNIbyte *)FruitSalad.Items.Islice;
      MniFile->ImageParameters[image].Iyoff  = *(MNIbyte *)FruitSalad.Items.Iyoff;
      MniFile->ImageParameters[image].Tsc    = convert_r4(FruitSalad.Items.Tsc);
      MniFile->ImageParameters[image].Isea   = convert_i2(FruitSalad.Items.Isea);
      MniFile->ImageParameters[image].Samp   = convert_r4(FruitSalad.Items.Samp);
      MniFile->ImageParameters[image].OffGry = convert_r4(FruitSalad.Items.OffGry);
      MniFile->ImageParameters[image].Isctim = convert_i2(FruitSalad.Items.Isctim);
      MniFile->ImageParameters[image].Swid   = convert_r4(FruitSalad.Items.Swid);
      MniFile->ImageParameters[image].SclGry = convert_r4(FruitSalad.Items.SclGry);
      MniFile->ImageParameters[image].RatLin = convert_i2(FruitSalad.Items.RatLin);
      MniFile->ImageParameters[image].OffWht = convert_r4(FruitSalad.Items.OffWht);
      MniFile->ImageParameters[image].RatRan = convert_i2(FruitSalad.Items.RatRan);
      MniFile->ImageParameters[image].SclWht = convert_r4(FruitSalad.Items.SclWht);
      
    }
    
  }
  
  free(image_base);
  
}

MniInfo *open_mni_file(char *mni_filename, Boolean dump, int dump_type)
{
  MniInfo *MniFile;
  MniHeader header;
  char matrix_type;
  
  MniFile = (MniInfo *)calloc(1,sizeof(MniInfo));
  MniFile->FileName = strdup(mni_filename);
  
  if ((MniFile->fp = fopen(MniFile->FileName, "r")) == NULL){
    printf("\nCould not open file %s.\n", MniFile->FileName);
    return(NULL);
  }
  
  (void) fread(&header, 1, sizeof(MniHeader), MniFile->fp);
  
  MniFile->PatientName       = convert_str(header.PatientName, NAME_LENGTH);
  MniFile->PatientNumber     = convert_str(header.PatientNumber, NUMBER_LENGTH);
  MniFile->CameraId          = convert_str(header.CameraId, ID_LENGTH);
  MniFile->AcquisitionDate   = convert_str(header.AcquisitionDate, TIME_DATE_LENGTH);
  MniFile->AcquisitionTime   = convert_str(header.AcquisitionTime, TIME_DATE_LENGTH);
  MniFile->PhysicianName     = convert_str(header.PhysicianName, NAME_LENGTH);
  MniFile->StudyType         = convert_str(header.StudyType, NUMBER_LENGTH);
  MniFile->CollimatorType    = convert_str(header.CollimatorType, 4);
  MniFile->Isotope           = convert_str(header.Isotope, NUMBER_LENGTH);
  MniFile->Dose              = convert_str(header.Dose, NUMBER_LENGTH);
  MniFile->InjectionTime     = convert_str(header.InjectionTime, TIME_DATE_LENGTH);
  
  MniFile->CommentOffset     = convert_i2(header.CommentOffset);
  MniFile->BlockCount        = convert_i2(header.BlockCount);
  MniFile->ReconMask         = convert_i2(header.ReconMask);
  MniFile->AdminOffset       = convert_i2(header.AdminOffset);
  MniFile->ScannerId         = convert_i2(header.ScannerId);
  MniFile->DataOffset        = convert_i2(header.DataOffset);
  MniFile->CountsOffset      = convert_i2(header.CountsOffset);
  MniFile->FrameCount        = convert_i2(header.FrameCount);
  MniFile->GroupCount        = convert_i2(header.GroupCount);
  MniFile->DelayTime         = convert_i2(header.DelayTime);
  MniFile->TiltAngle         = convert_i2(header.TiltAngle);
  MniFile->AcquisitionStatus = convert_i2(header.AcquisitionStatus);
  
  if (MniFile->FrameCount < 1 || MniFile->FrameCount > 512){
    printf("\n%s: bad file format.\n", MniFile->FileName);
    return(NULL);
  }
  
  switch(MniFile->ScannerId){
  case 1:
    MniFile->ScannerType = strdup("MRI");
    MniFile->Slices = MniFile->FrameCount;
    MniFile->Frames = 1;
    break;
  case 2:
    MniFile->ScannerType = strdup("SCX");
    if (MniFile->FrameCount % SCX_SLICES != 0){
      MniFile->Slices = MniFile->FrameCount;
      MniFile->Frames = 1;
    }else{
      MniFile->Slices = SCX_SLICES;
      MniFile->Frames = MniFile->FrameCount / SCX_SLICES;
    }
    break;
  case 3:
    MniFile->ScannerType = strdup("STX");
    MniFile->Slices = MniFile->FrameCount;
    MniFile->Frames = 1;
    break;
  default:
    MniFile->ScannerType = strdup("???");
    MniFile->Slices = MniFile->FrameCount;
    MniFile->Frames = 1;
    break;
  }
  
  matrix_type = *(char *)(header.Groups[0].MatrixType);
  if (matrix_type < 0x30) matrix_type += 0x30;
  MniFile->MatrixType = matrix_type;
  
  switch (matrix_type){
  case '3':
    MniFile->ImageWidth = 64;
    MniFile->PixelWidth = 1;
    break;
  case '4':
    MniFile->ImageWidth = 64;
    MniFile->PixelWidth = 2;
    break;
  case '5': 
    MniFile->ImageWidth = 128;
    MniFile->PixelWidth = 1;
    break;
  case '6':
    MniFile->ImageWidth = 128;
    MniFile->PixelWidth = 2;
    break;      
  case '7':
    MniFile->ImageWidth = 256;
    MniFile->PixelWidth = 1;
    break;
  case '8':
    MniFile->ImageWidth = 256;
    MniFile->PixelWidth = 2;
    break;
  }
  
  MniFile->ImageHeight = MniFile->ImageWidth;
  MniFile->ImageSize =  MniFile->ImageWidth * MniFile->ImageHeight;
  MniFile->ImageBytes =  MniFile->ImageSize * MniFile->PixelWidth;

  MniFile->ImageParameters = (ImageInfo *)calloc(MniFile->FrameCount,sizeof(ImageInfo));
  
  if (dump && dump_type & MNI_DUMP_HEADER){
    
    (void) printf("File name:          %s\n", MniFile->FileName);
    (void) printf("Patient name:       %s\n", MniFile->PatientName);
    (void) printf("Patient Number:     %s\n", MniFile->PatientNumber);
    (void) printf("Camera id:          %s\n", MniFile->CameraId);
    (void) printf("Aquisition date:    %s\n", MniFile->AcquisitionDate);
    (void) printf("Aquisition time:    %s\n", MniFile->AcquisitionTime);
    (void) printf("Physicians name:    %s\n", MniFile->PhysicianName);
    (void) printf("Study type:         %s\n", MniFile->StudyType);
    (void) printf("Collimator type:    %s\n", MniFile->CollimatorType);
    (void) printf("Isotope:            %s\n", MniFile->Isotope);
    (void) printf("Dose:               %s\n", MniFile->Dose);
    (void) printf("InjectionTime:      %s\n", MniFile->InjectionTime);
    
    (void) printf("Comment offset:     %d\n", MniFile->CommentOffset);
    (void) printf("Block Count:        %d\n", MniFile->BlockCount);
    (void) printf("Recon Mask:         %d\n", MniFile->ReconMask);
    (void) printf("Admin offset:       %d\n", MniFile->AdminOffset);
    (void) printf("Scanner Id:         %d\n", MniFile->ScannerId);
    (void) printf("Data offset:        %d\n", MniFile->DataOffset);
    (void) printf("Counts offset:      %d\n", MniFile->CountsOffset);
    (void) printf("Total Frames:       %d\n", MniFile->FrameCount);
    (void) printf("Total groups:       %d\n", MniFile->GroupCount);
    (void) printf("Delay time:         %d\n", MniFile->DelayTime);
    (void) printf("Tilt angle:         %d\n", MniFile->TiltAngle);
    (void) printf("Acquisition status: %d\n", MniFile->AcquisitionStatus);
    
    (void) printf("Scanner type:       %s\n", MniFile->ScannerType);
    (void) printf("Slices:             %d\n", MniFile->Slices);
    (void) printf("Frames:             %d\n", MniFile->Frames);
    (void) printf("Image width:        %d\n", MniFile->ImageWidth);
    (void) printf("Image height:       %d\n", MniFile->ImageHeight);
    (void) printf("Pixel width:        %d\n", MniFile->PixelWidth);
    (void) printf("Image size:         %d\n", MniFile->ImageSize);
    (void) printf("Storage per image:  %d\n", MniFile->ImageBytes);
    
  }

  return(MniFile);
  
}

void read_mni_parameters(MniInfo *MniFile,
                         int     *slices,
                         int     slice_count,
                         int     *frames,
                         int     frame_count,
                         Boolean dump,
                         int     dump_type)
{

  int image, frame, slice;
  
  read_fruit_salads(MniFile,
                    slices,
                    slice_count,
                    frames,
                    frame_count);
  
  if (dump){
    
    int present_slice, present_frame;
    
    if  (dump_type & MNI_DUMP_TABLE){ 
      (void) printf("\n");
      (void) printf(" Image | Frame | Slice |  QSC  | ISEA | zpos |  TSC  | ISCTIM\n");
      (void) printf("--------------------------------------------------------------\n");
    }
    
    for (slice = 0; slice < slice_count; slice++){
      
      for (frame = 0; frame < frame_count; frame++){
        
        present_slice = slices[slice]-1;
        present_frame = frames[frame]-1;
        image = MniFile->Frames*present_slice + present_frame;
        
        if (dump_type & MNI_DUMP_TABLE){
          
          (void) printf("  %3d  |", image + 1);
          (void) printf("  %2d   |", present_frame + 1);
          (void) printf("  %2d   |", present_slice + 1);
          (void) printf(" %.3f |", MniFile->ImageParameters[image].Qsc);
          (void) printf(" %3d  |", MniFile->ImageParameters[image].Isea);
          (void) printf(" %3d  |", MniFile->ImageParameters[image].Izpos);
          (void) printf(" %4.1f  |", MniFile->ImageParameters[image].Tsc);
          (void) printf("  %3d  \n", MniFile->ImageParameters[image].Isctim);
          
        }
        
        if (dump_type & MNI_DUMP_SHORT){
          
          if (dump_type & MNI_DUMP_HEADER) (void) printf("image: ");
          (void) printf("%3d ", image + 1);
          (void) printf("%2d ", frame + 1);
          (void) printf("%2d ", slice + 1);
          (void) printf("%.5f ", MniFile->ImageParameters[image].Qsc);
          (void) printf("%3d ", MniFile->ImageParameters[image].Isea);
          (void) printf("%3d ", MniFile->ImageParameters[image].Izpos);
          (void) printf("%5.2f ", MniFile->ImageParameters[image].Tsc);
          (void) printf("%4d\n", MniFile->ImageParameters[image].Isctim);
          
        }
        
        if (dump_type & MNI_DUMP_LONG){
          
          (void) printf("image: %d\n", image + 1);
          (void) printf("       Qsc: %.4f\n", MniFile->ImageParameters[image].Qsc);
          (void) printf("    CntImg: %.0f\n", MniFile->ImageParameters[image].CntImg);
          (void) printf("      Zpos: %.0f\n", MniFile->ImageParameters[image].Zpos);
          (void) printf("     Izpos: %d\n",   MniFile->ImageParameters[image].Izpos);
          (void) printf("     Itype: %d\n",   MniFile->ImageParameters[image].Itype);
          (void) printf("       Isc: %d\n",   MniFile->ImageParameters[image].Isc);
          (void) printf("    Islice: %d\n",   MniFile->ImageParameters[image].Islice);
          (void) printf("     Iyoff: %d\n",   MniFile->ImageParameters[image].Iyoff);
          (void) printf("       Tsc: %.1f\n", MniFile->ImageParameters[image].Tsc);
          (void) printf("      Isea: %d\n",   MniFile->ImageParameters[image].Isea);
          (void) printf("      Samp: %.4f\n", MniFile->ImageParameters[image].Samp);
          (void) printf("    OffGry: %.0f\n", MniFile->ImageParameters[image].OffGry);
          (void) printf("    Isctim: %d\n",   MniFile->ImageParameters[image].Isctim);
          (void) printf("      Swid: %.4f\n", MniFile->ImageParameters[image].Swid);
          (void) printf("    SclGry: %.4f\n", MniFile->ImageParameters[image].SclGry);
          (void) printf("    RatLin: %d\n",   MniFile->ImageParameters[image].RatLin);
          (void) printf("    OffWht: %.0f\n", MniFile->ImageParameters[image].OffWht);
          (void) printf("    RatRan: %d\n",   MniFile->ImageParameters[image].RatRan);
          (void) printf("    SclWht: %.4f\n", MniFile->ImageParameters[image].SclWht);
          
        }
      }
    }
  }
}

void close_mni_file(MniInfo *MniFile)
{
  (void) fclose(MniFile->fp);
  free(MniFile->FileName);
  free(MniFile->PatientName);
  free(MniFile->PatientNumber);
  free(MniFile->CameraId);
  free(MniFile->AcquisitionTime);
  free(MniFile->AcquisitionDate);
  free(MniFile->PhysicianName);
  free(MniFile->StudyType);
  free(MniFile->CollimatorType);
  free(MniFile->Isotope);
  free(MniFile->Dose);
  free(MniFile->InjectionTime);
  free(MniFile->ScannerType);
  free(MniFile->ImageParameters);
  free(MniFile);
}

zero_fruit_salad(MNIbyte *outbuf, short set_point, int line_size)
{
  int line;
  for (line = 0; line < FRUIT_SALAD_HEIGHT; line++)
    (void) memset(&outbuf[line_size * line], (int) set_point, FRUIT_SALAD_WIDTH);
}

void get_mni_slice(MniInfo *MniFile,
                   int slice,
                   int frame,
                   void *out_image,
                   int precision,
                   Boolean scale)
{
  
  int image, pixel;
  MNIbyte *raw_image;
  
  raw_image = (MNIbyte *)malloc(MniFile->ImageSize * MniFile->PixelWidth);
  image = MniFile->Frames * slice + frame;
  (void) fseek(MniFile->fp, 
               MniFile->DataOffset * BLOCK_SIZE + image * MniFile->ImageBytes, 
               SEEK_SET);
  (void) fread(raw_image, sizeof(MNIbyte), MniFile->ImageBytes, MniFile->fp);
  zero_fruit_salad(raw_image, (short) 0, MniFile->ImageWidth * MniFile->PixelWidth);
  
  if (MniFile->PixelWidth == 1){
    
    switch(precision){
      
    case MNIdouble:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((double *)out_image)[pixel] = (double)
            (MniFile->ImageParameters[image].Qsc * 
             (float)((short)raw_image[pixel] - MniFile->ImageParameters[image].Isea));
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((double *)out_image)[pixel] = (double)raw_image[pixel];
      break;
      
    case MNIfloat:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((float *)out_image)[pixel] =
            MniFile->ImageParameters[image].Qsc * 
              (float)((short)raw_image[pixel] - MniFile->ImageParameters[image].Isea);
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((float *)out_image)[pixel] = (float)raw_image[pixel];
      break;
      
    case MNIlong:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((long *)out_image)[pixel] = (long)
            (MniFile->ImageParameters[image].Qsc * 
             (float)((short)raw_image[pixel] - MniFile->ImageParameters[image].Isea));
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((long *)out_image)[pixel] = (long)raw_image[pixel];
      break;
      
    case MNIshort:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((short *)out_image)[pixel] = (short)
            (MniFile->ImageParameters[image].Qsc * 
             (float)((short)raw_image[pixel] - MniFile->ImageParameters[image].Isea));
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((short *)out_image)[pixel] = (short)raw_image[pixel];
      break;
      
    case MNIchar:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((MNIbyte *)out_image)[pixel] = (MNIbyte)
            (MniFile->ImageParameters[image].Qsc * 
             (float)((short)raw_image[pixel] - MniFile->ImageParameters[image].Isea));
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((MNIbyte *)out_image)[pixel] = (MNIbyte)raw_image[pixel];
      break;
    }
    
  }else if (MniFile->PixelWidth == 2){
    
    switch(precision){
      
    case MNIdouble:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((double *)out_image)[pixel] = (double)
            (MniFile->ImageParameters[image].Qsc * 
             (float)(((short *)raw_image)[pixel] - MniFile->ImageParameters[image].Isea));
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((double *)out_image)[pixel] = (double)((short *)raw_image)[pixel];
      break;
      
    case MNIfloat:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((float *)out_image)[pixel] =
            MniFile->ImageParameters[image].Qsc * 
              (float)(((short *)raw_image)[pixel] - MniFile->ImageParameters[image].Isea);
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((float *)out_image)[pixel] = (float)((short *)raw_image)[pixel];
      break;
      
    case MNIlong:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((long *)out_image)[pixel] = (long)
            (MniFile->ImageParameters[image].Qsc * 
             (float)(((short *)raw_image)[pixel] - MniFile->ImageParameters[image].Isea));
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((long *)out_image)[pixel] = (long)((short *)raw_image)[pixel];
      break;
      
    case MNIshort:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((short *)out_image)[pixel] = (short)
            (MniFile->ImageParameters[image].Qsc * 
             (float)(((short *)raw_image)[pixel] - MniFile->ImageParameters[image].Isea));
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((short *)out_image)[pixel] = ((short *)raw_image)[pixel];
      break;
      
    case MNIchar:
      if (scale)
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((MNIbyte *)out_image)[pixel] = (MNIbyte)
            (MniFile->ImageParameters[image].Qsc * 
             (float)(((short *)raw_image)[pixel] - MniFile->ImageParameters[image].Isea));
      else 
        for (pixel = 0; pixel < MniFile->ImageSize; pixel++)
          ((MNIbyte *)out_image)[pixel] = (MNIbyte)((short *)raw_image)[pixel];
      break;
      
    }
    
  }
  
  free(raw_image);
  
}
