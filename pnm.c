/**
 * \file pnm.c
 * \brief Ce fichier contient les définitions de types et les fonctions de manipulation d'images PNM.
 * \author: Russe Cyril s170220
 * \date: 26-03-2020
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "pnm.h"
#include "filtre.h"

/**
 * \struct PNM_t
 * \brief Définition du type opaque PNM
 */
struct PNM_t {
   int format;
   int nbr_ligne, nbr_colonne;
   unsigned int valeur_max;
   unsigned short ***valeurs_pixel;
};


int load_pnm(PNM **image, char* filename) {
   int type_image, extension_fichier;
   int nbr_ligne, nbr_colonne;
   unsigned int valeur_max;
   assert(filename!=NULL);

   FILE* fichier = fopen(filename, "r");//ouverture du fichier
   if (fichier==NULL){
      printf("Impossible d'ouvrir le fichier %s.\n", filename);
      return -2;
   }

   //Vérifications format
   if (verifie_nombre_magique(&type_image, fichier)==-1){
      printf("L'en tête de l'image est malformée.\n");
      fclose(fichier);
      return -3;
   }

   //vérifie que le format lu dans l'en tête du fichier correspond bien à l'extension de filename
   if (verifie_correspondance_extension_format(type_image, filename, &extension_fichier)==-1){
      printf("L'extension de %s ne correspond pas au format de l'en tête.\n", filename);
      fclose(fichier);
      return -2;
   }
   //enregistrement dimensions
   if(lit_dimensions_image(&nbr_ligne, &nbr_colonne, fichier)==-1){
      printf("En tête de fichier mal formée. Impossible de lire les dimensions.\n");
      fclose(fichier);
      return -3;
   }

   //enregistrement valeur max
   if(type_image==2 || type_image==3){
      if(lit_valeur_max(&valeur_max, fichier)==-1){
         printf("En tête de fichier mal formée. Impossible de lire la valeur max.\n");
         fclose(fichier);
         return -3;
      }
   }

   /*allocation dynamique d'une struct PNM et allocation du tableau qui contiendra les valeurs de chaque pixel de l'image
      remplissage de la structure (informations + valeurs de chaque pixel)*/
   *image = constructeur_PNM(nbr_ligne, nbr_colonne, type_image, valeur_max);
   if (*image==NULL){
      printf("Allocation de mémoire impossible.\n");
      fclose(fichier);
      return -1;
   }

   if(charge_valeurs_fichier(*image, fichier)==-1){
      libere_PNM(image);
      printf("Erreur lors du chargement de l'image.\n");
      return -2;
   }

   fclose(fichier);
   return 0;
}

PNM *constructeur_PNM(int nbr_ligne,int nbr_colonne, int format, unsigned int valeur_max){
   int i, j;

   //malloc la matrice et chaque élément de celle-ci
   PNM *image = malloc(sizeof(PNM));
   if (image==NULL)
      return NULL;

   //malloc du pointeur vers le tableau de pointeur qui vont eux mêmes pointer vers un tableau allant de 1 short(PBM, PGM) à 3 short(PPM)
   image->valeurs_pixel = malloc(nbr_ligne * sizeof(unsigned short**));
   if(image->valeurs_pixel==NULL){
      free(image);//libere la struct si impossibilité d'allouer de la mémoire pour le tableau de pointeurs de short
      return NULL;
   }

   //allocation des pointeurs vers short (1 si PBM,PGM; 3 si PPM)
   for(i=0; i<nbr_ligne; i++){
      image->valeurs_pixel[i] = malloc(nbr_colonne * sizeof(unsigned short*));//allocation d'un pointeur d'une ligne
      if (image->valeurs_pixel[i]==NULL){//si échec de l'allocation de la ligne, free de tout ce qui a été allouer avant cette ligne
         for(i--; i>=0; i--){//free reste du tableau avant la ligne n'ayant pu être malloc
            for(j=0; j<nbr_colonne; j++)
               free(image->valeurs_pixel[i][j]);
            free(image->valeurs_pixel[i]);
         }
         free(image->valeurs_pixel);
         free(image);
         image=NULL;
         return NULL;//sortie de la fonction car échec d'allocation d'un pointeur de ligne
      }
      else{//si allocation de la ligne avec succès, allocation des pointeurs de chaque élément de cette ligne
         for(j=0; j<nbr_colonne; j++){
            if(image->format==3)//si PPM, allocation de 3 cases pour pouvoir enregister 3 valeurs, représentant les correspondances en RGB de chaque pixel
               image->valeurs_pixel[i][j] = malloc(3 * sizeof(unsigned short));
            else//PBM, PGM : un seul short nécessaire pour chaque pixel
               image->valeurs_pixel[i][j] = malloc(sizeof(unsigned short));

            if(image->valeurs_pixel[i][j]==NULL){//si échec de l'allocation de la case du/des short, alors free de ce qui a déjà été alloué
               for(j--; j>=0; j--)//free éléments de la colonne avant l'élément n'ayant pu être malloc
                  free(image->valeurs_pixel[i][j]);
               free(image->valeurs_pixel[i]);

               for(i--;i>=0; i--){//free reste du tableau avant la colonne dans laquelle un élément n'a pu être malloc
                  for(j=0; j<nbr_colonne; j++)
                     free(image->valeurs_pixel[i][j]);
                  free(image->valeurs_pixel[i]);
               }
            free(image->valeurs_pixel);
            free(image);
            image=NULL;
            return NULL;//sortie de la fonction car échec de l'allocation de la case de short du tableau
            }
         }
      }
   }

   //initialisation des informations de l'image dans la struct PNM
   image->nbr_ligne = nbr_ligne;
   image->nbr_colonne = nbr_colonne;
   image->format = format;
   if(image->format == 1)
      image->valeur_max = 1;
   else
      image->valeur_max = valeur_max;

   return image;
}

int charge_valeurs_fichier(PNM *image, FILE *fichier){
   assert(image!=NULL && fichier!=NULL);
   char stockage_valeur_fichier[100];
   int i, j, nbr_valeur_ppm = 0;

   //initialisation des valeurs du tableau de pixel représentant l'image
   for (i = 0; i < image->nbr_ligne; i++){
      for (j = 0; j < image->nbr_colonne;){
         if(fscanf(fichier, "%s", stockage_valeur_fichier)==0)
            return -1;
         if (stockage_valeur_fichier[0] != '#'){
            if (atoi(stockage_valeur_fichier) > (int)image->valeur_max){
               libere_PNM(&image);
               return -1;
            }
            if (image->format == 1 || image->format == 2){
               image->valeurs_pixel[i][j][0] = atoi(stockage_valeur_fichier);
               j++;
            }
            else{
               image->valeurs_pixel[i][j][nbr_valeur_ppm] = atoi(stockage_valeur_fichier);
               if (nbr_valeur_ppm == 2)
                  j++;
               nbr_valeur_ppm++;
               nbr_valeur_ppm %= 3;
            }
         }
         else
            fscanf(fichier, "%*[^\n]");
      }
   }

   return 0;
}

int acces_nbr_ligne_PNM(PNM *image){
   assert(image!=NULL);

   return image->nbr_ligne;
}

int acces_nbr_colonne_PNM(PNM *image){
   assert(image!=NULL);

   return image->nbr_colonne;
}

int acces_format_PNM(PNM *image){
   assert(image!=NULL);

   return image->format;
}

unsigned int acces_valeur_max_PNM(PNM *image){
   assert(image!=NULL);

   return image->valeur_max;
}

unsigned short ***acces_valeurs_pixel_PNM(PNM *image){
   assert(image!=NULL);

   return (image->valeurs_pixel);
}

void changer_valeur_pixel_PNM(PNM *image, int numero_ligne, int numero_colonne, unsigned short valeur[]){
   assert(image!=NULL);
   
   if(image->format==3){
      for(int i=0; i<3; i++)
         image->valeurs_pixel[numero_ligne][numero_colonne][i]=valeur[i];
   }
   else
      image->valeurs_pixel[numero_ligne][numero_colonne][0]=valeur[0];
}

void changer_format(PNM *image, int format){
   assert(image!=NULL && (format==1||format==2||format==3));

   image->format=format;
}

void libere_PNM(PNM **image){
   if(*image!=NULL)//vérification de la validité du pointeur avant de le free
   {
      for(int i=0; i<(*image)->nbr_ligne; i++){
         for(int j=0; j<(*image)->nbr_colonne; j++)
            free((*image)->valeurs_pixel[i][j]);
         free((*image)->valeurs_pixel[i]);
      }
      free((*image)->valeurs_pixel);
      free(*image);
   }
   *image=NULL;
}

int lit_valeur_max(unsigned int *valeur_max, FILE  *fichier){
   char contenu_fichier[100];
   int nbr_fscanf = 0;
   do{
      if(contenu_fichier[0]!='#'){//vérifie que le premier caractère n'est pas un '#', si c'est le cas alors c'est un commentaire donc ignore la ligne
         nbr_fscanf = fscanf(fichier, "%s[^\n]", contenu_fichier);
         if (atoi(contenu_fichier)<=255 && atoi(contenu_fichier)>=0){
            *valeur_max = atoi(contenu_fichier);
            return 0;
         }
         else
            return -1;
      }
      else
         fscanf(fichier, "%*[^\n]");//instruction permettant d'ignorer une ligne complète dans le cas d'un commentaire
      
   }while(nbr_fscanf>0);//lecture tant qu'il y a quelque chose à lire

   return -1;//si sorti de la boucle alors la valeur n'a pas été enregistrée alors qu'il n'y a plus rien à lire, retourne -1
}

int lit_dimensions_image(int *nbr_ligne, int *nbr_colonne, FILE *fichier){
   unsigned int n = 0;
   char contenu_fichier[100];
   int nbr_fscanf = 0; 
   do{
      if (contenu_fichier[0]=='#')//si premier caractère est un '#' alors la ligne est un commentaire
         nbr_fscanf = fscanf(fichier, "%*[^\n]");//instruction permettant d'ignorer une ligne complète dans le cas d'un commentaire
      
      nbr_fscanf = fscanf(fichier, "%s[^\n]", contenu_fichier);
      if (contenu_fichier[0]!='#'){
         n++;
         if(n==1){
            *nbr_ligne = atoi(contenu_fichier);
            if (*nbr_ligne == 0)
               return -1;
         }
         else if(n==2){
            *nbr_colonne = atoi(contenu_fichier);
            if (*nbr_colonne == 0)
               return -1;
            return 0;
         }
      }
   }while(nbr_fscanf>0);//boucle tant qu'il y a quelque chose à lire
   
   return -1;//si sorti de la boucle alors la valeur n'a pas été enregistrée alors qu'il n'y a plus rien à lire, retourne -1
}

int verifie_nombre_magique(int *type, FILE*  fichier){

   unsigned int numero_ligne = 0;
   char contenu_fichier[100];
   int nbr_fscanf = 0;

   do{
      if (contenu_fichier[0]=='#')
         nbr_fscanf = fscanf(fichier, "%*[^\n]");
      
      nbr_fscanf = fscanf(fichier, "%s[^\n]", contenu_fichier);
         
      if (contenu_fichier[0]!='#'){
         numero_ligne++;
         if (contenu_fichier[0]!='P')
            return -1;
         if (contenu_fichier[1]=='1'){
            *type =  1;
            return 0;
         }
         else if (contenu_fichier[1]=='2'){
            *type = 2;
            return 0;
         }
         else if (contenu_fichier[1]=='3'){
            *type = 3;
            return 0;
         }
         else 
            return -1;
      }

   }while(numero_ligne==0 && nbr_fscanf>0);

   return -1;//si sorti de la boucle alors la valeur n'a pas été enregistrée alors qu'il n'y a plus rien à lire, retourne -1
}

int verifie_correspondance_extension_format(int type_image, char *filename, int *extension_fichier){
   if(filename==NULL)
      return -1;
   int taille_nom=0;

   while(filename[taille_nom]!='\0')
      taille_nom++;

   //lis l'extension du fichier si on a la certitude que le nom de fichier a une taille suffisante pour que l'extension puisse exister
   if(taille_nom>4){
      if (filename[taille_nom-4]=='.' && filename[taille_nom-3]=='p' && filename[taille_nom-2]=='b' && filename[taille_nom-1]=='m')
         *extension_fichier = 1;
      else if (filename[taille_nom-4]=='.' && filename[taille_nom-3]=='p' && filename[taille_nom-2]=='g' && filename[taille_nom-1]=='m')
         *extension_fichier = 2;
      else if (filename[taille_nom-4]=='.' && filename[taille_nom-3]=='p' && filename[taille_nom-2]=='p' && filename[taille_nom-1]=='m')
         *extension_fichier = 3;
      else
         *extension_fichier = -1;
   }
   else
      return -1;

   if(*extension_fichier == type_image)//retourne 0 si l'extension correspond au format donné en argument, -1 sinon
      return 0;
   else
      return -1;

}

int write_pnm(PNM *image, char* filename) {
   FILE *fichier;
   int extension_fichier;
   if(image==NULL)
      return -2;

   //vérifie que l'extension du nom donnée pour le fichier d'écriture correspond au format de l'image à écrire
   if(verifie_correspondance_extension_format(image->format, filename, &extension_fichier)==-1){
      printf("L'extension du fichier dans lequel copier l'image ne correspond pas au format de celle-ci.\n");
      return -1;
   }
   //vérifie que le nom de fichier ne contient pas des caractères interdits
   if(verifie_validite_filename(filename)==-1){
      printf("Impossible de copier l'image. Le nom contient des caractères interdits.\n");
      return -1;
   }
   fichier = fopen(filename, "w");//ouvre le fichier d'écriture en mode "write"
   if (fichier==NULL){
      printf("Impossible d'ouvrir le fichier afin d'y copier l'image.\n");
      return -2;
   }
   //écrit l'en tête du fichier 
   if(ecrit_en_tete_fichier_PNM(image, fichier)==-1){
      printf("Impossible d'écrire l'en tête.\n");
      fclose(fichier);
      return -2;
   }
   //écrit les valeurs de chaque pixel dans le fichier
   if(ecrit_image_dans_fichier(image, fichier)==-1){
      printf("Un problème est survenu lors de l'écriture de l'image.\n");
      fclose(fichier);
      return -2;
   }
   
   fclose(fichier);
   
   return 0;
}

int ecrit_image_dans_fichier(PNM *image, FILE *fichier){
    for(int i=0; i<image->nbr_ligne; i++){
      for(int j=0; j<image->nbr_colonne; j++){
         if(image->format==3){
            for(int x=0; x<3; x++)
               fprintf(fichier, "%hu ", image->valeurs_pixel[i][j][x]);
         }
         else
            fprintf(fichier, "%hu ", image->valeurs_pixel[i][j][0]);
      }
      fprintf(fichier, "\n");
   }
   return 0;
}

int verifie_validite_filename(char *filename){
   assert(filename!=NULL);
   char caractere=1;
   int i=0;
   while(caractere!='\0'){
      caractere=filename[i];
      if(caractere=='/' || caractere=='\\' || caractere==':' || caractere=='*' || caractere=='?' || caractere=='"' || caractere=='<' || caractere=='>' || caractere=='|')
         return -1;
      i++;
   }
   return 0;
}

int ecrit_en_tete_fichier_PNM(PNM *image, FILE *fichier){
   assert(image!=NULL && fichier!=NULL);
   switch (image->format)
   {
   case 1:
      fprintf(fichier, "P1\n");
      break;
   case 2:
      fprintf(fichier, "P2\n");
      break;
   case 3:
      fprintf(fichier, "P3\n");
      break;
   default:
      return -1;
   }

   fprintf(fichier, "%d %d\n", image->nbr_ligne, image->nbr_colonne);

   if(image->format!=1)
      fprintf(fichier, "%d\n", image->valeur_max);

   return 0;

}

int verifie_extension_fichier(char *filename, PNM *image){
   assert(filename!=NULL);
   int format=acces_format_PNM(image);

   int taille=strlen(filename);
   if(taille>5){
      if(filename[taille-4]!='.'){
         printf("Extension du fichier %s incorrect.\n", filename);
         return -1;
      }
      else{
         
         filename[taille-3]='p';
         filename[taille-1]='m';
         switch (format)
         {
         case 1:
            printf("1\n");
            filename[taille-2]='b';
            break;
         case 2:
            printf("2\n");
            filename[taille-2]='g';
            break;
         case 3:
            printf("3\n");
            filename[taille-2]='p';
            break;
         }
      }
   }
   else
      return -1;

   return 0;
}

