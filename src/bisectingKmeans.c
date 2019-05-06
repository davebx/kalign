#include <xmmintrin.h>


#include "sequence_distance.h"
#include "bisectingKmeans.h"

#include "euclidean_dist.h"

#include "alignment.h"
#include "pick_anchor.h"
struct node{
        struct node* left;
        struct node* right;
        int id;
};



struct node* alloc_node(void);

int label_internal(struct node*n, int label);
int* readbitree(struct node* p,int* tree);
void printTree(struct node* curr,int depth);
struct node* bisecting_kmeans(struct node* n, float** dm,int* samples,int numseq, int num_anchors,int num_samples,struct drand48_data* randBuffer);

float** pair_wu_fast_dist(struct alignment* aln, struct aln_param* ap, int* num_anchors);

int unit_zero(float** d, int len_a, int len_b);

int build_tree_kmeans(struct alignment* aln,struct parameters* param, struct aln_param* ap)
{
        struct drand48_data randBuffer;
        struct node* root = NULL;
        float** dm = NULL;
        int* tree = NULL;
        int* samples = NULL;
        int num_anchors;
        int numseq;

        int i;

        ASSERT(aln != NULL, "No alignment.");
        ASSERT(param != NULL, "No input parameters.");
        ASSERT(ap != NULL, "No alignment parameters.");

        srand48_r(time(NULL), &randBuffer);

        tree = ap->tree;

        numseq = aln->numseq;

        dm = pair_wu_fast_dist(aln, ap, &num_anchors);


        unit_zero(dm, aln->numseq, num_anchors);

        /*int j;
        double r;
        float sum = 0.0;
        for(i = 0; i < aln->numseq;i++){
                sum = 0.0;
                for(j = 0; j < num_anchors;j++){
                        //drand48_r(&randBuffer, &r);
                        sum += dm[i][j];
                        //fprintf(stdout,"%f ",dm[i][j]);

                }
                if(sum == 0.0){
                        sum = 1.0;
                }
                for(j = 0; j < num_anchors;j++){
                        //drand48_r(&randBuffer, &r);
                        dm[i][j] = dm[i][j] / sum;
                        fprintf(stdout,"%f ",dm[i][j]);

                }
                fprintf(stdout,"\n");
                }

        */
        //RUNP(dm = pair_aln_dist(aln, ap, &num_anchors));

        /*for(i = 0; i < aln->numseq;i++){
                fprintf(stdout,"%d",i);
                for(j = 0; j <  num_anchors;j++){
                        fprintf(stdout," %f", dm[i][j]);
                }
                fprintf(stdout,"\n");
                }*/

        MMALLOC(samples, sizeof(int)* numseq);
        for(i = 0; i < numseq;i++){
                samples[i] = i;
        }

        RUNP(root = alloc_node());




        RUNP(root = bisecting_kmeans(root, dm, samples, numseq, num_anchors, numseq, &randBuffer));

        label_internal(root, numseq);
        //printTree(root, 0);

        ap->tree[0] = 1;
        ap->tree = readbitree(root, ap->tree);
        for (i = 0; i < (numseq*3);i++){
                tree[i] = tree[i+1];
                //fprintf(stdout,"%d %d\n",tree[i], aln->num_profiles);
                //if(tree[i] = aln->num_profiles-1){
                //break;
                //}
        }

        MFREE(root);
        for(i =0 ; i < aln->numseq;i++){
                _mm_free(dm[i]);
        }
        MFREE(dm);

        return OK;
ERROR:
        return FAIL;
}

int unit_zero(float** d, int len_a, int len_b)
{

        float sum;
        float sq_sum;
        float mean;
        float variance;

        int i,j;

        for(j = 0; j < len_b;j++){
                sum = 0.0f;
                sq_sum = 0.0f;
                for(i = 0; i < len_a;i++){
                        sum += d[i][j];
                        sq_sum += d[i][j] * d[i][j];
                }
                mean = sum / (float) len_a;
                variance = sq_sum / (float) len_a - mean * mean;
                //fprintf(stdout,"%d %f %f \n",j,mean,variance);
                if(variance){
                for(i = 0; i < len_a;i++){
                        d[i][j] = (d[i][j] - mean) / variance;
                }
                }
                /*sum = 0.0f;
                sq_sum = 0.0f;
                for(i = 0; i < len_a;i++){
                        sum += d[i][j];
                        sq_sum += d[i][j] * d[i][j];
                }
                mean = sum / (float) len_a;
                variance = sq_sum / (float) len_a - mean * mean;
                fprintf(stdout,"%d %f %f \n",j,mean,variance);*/
        }
        return OK;
}

float** pair_wu_fast_dist(struct alignment* aln, struct aln_param* ap, int* num_anchors)
{
        float** dm = NULL;
        int* anchors = NULL;
        ASSERT(aln != NULL,"No alignment");




        RUNP(anchors = pick_anchor(aln, num_anchors));

        //dm = protein_wu_distance(aln, 59.0,0, anchors, *num_anchors);

        dm = kmer_distance(aln,  anchors, *num_anchors,6);
        /* normalize  */
        MFREE(anchors);
        return dm;
ERROR:
        return NULL;
}


struct node* bisecting_kmeans(struct node* n, float** dm,int* samples,int numseq, int num_anchors,int num_samples,struct drand48_data* randBuffer)
{
        //long int r;
        int* sl = NULL;
        int* sr = NULL;
        int num_l,num_r;
        float* w = NULL;
        float* wl = NULL;
        float* wr = NULL;
        float* cl = NULL;
        float* cr = NULL;
        float dl = 0.0f;
        float dr = 0.0f;
        int i,j,s;
/* Pick random point (in samples !! ) */
        int stop = 0;
        if(num_samples == 1){
                n->id = samples[0];
                MFREE(samples);
                return n;
        }
        if(num_samples == 2){
                n->left = alloc_node();
                n->right = alloc_node();
                n->left->id = samples[0];
                n->right->id = samples[1];
                MFREE(samples);
                return n;

        }
        s = num_anchors / 8;
        if( num_anchors%8){
                s++;
        }
        s = s << 3;

        w = _mm_malloc(sizeof(float)*s,32);
        wr = _mm_malloc(sizeof(float) *s,32);
        wl = _mm_malloc(sizeof(float) *s,32);

        cr = _mm_malloc(sizeof(float) *s,32);
        cl = _mm_malloc(sizeof(float) *s,32);

        MMALLOC(sl, sizeof(int) * num_samples);
        MMALLOC(sr, sizeof(int) * num_samples);

        for(i = 0; i < s;i++){
                w[i] = 0.0f;
                wr[i] = 0.0f;
                wl[i] = 0.0f;
                cr[i] = 0.0f;
                cl[i] = 0.0f;
        }
        for(i = 0; i < num_samples;i++){
                s = samples[i];
                for(j = 0; j < num_anchors;j++){
                        w[j] += dm[s][j];
                }
        }
        for(j = 0; j < num_anchors;j++){
                w[j] /= num_samples;
        }


        //lrand48_r(randBuffer, &r);
        //r = r % num_samples;
        s = samples[0];
        //LOG_MSG("Selected %d\n",s);
        for(j = 0; j < num_anchors;j++){
                cl[j] = dm[s][j];
        }

        for(j = 0; j < num_anchors;j++){
                cr[j] = w[j] - (cl[j] - w[j]);
                //      fprintf(stdout,"%f %f  %f\n", cl[j],cr[j],w[j]);
        }



        /* check if cr == cl - we have identical sequences  */
        s = 0;
        //LOG_MSG("COMP");
        for(j = 0; j < num_anchors;j++){
                //fprintf(stdout,"%d\t%f %f  %f %d %e\n",j, cl[j],cr[j],w[j],s,cl[j] - cr[j]);
                //if(cl[j] != cr[j]){
                //fprintf(stdout," diff: %e  cutoff %e\n",fabsf(cl[j]-cr[j]),1.0E-5);
                if(fabsf(cl[j]-cr[j]) >  1.0E-6){
                        s = 1;
                        break;
                }

        }
        _mm_free(w);
        if(!s){
                //      LOG_MSG("Identical!!!");
                num_l = 0;
                num_r = 0;
                sl[num_l] = samples[0];
                num_l++;

                for(i =1 ; i <num_samples;i++){
                        sr[num_r] = samples[i];
                        num_r++;
                }
                _mm_free(wr);
                _mm_free(wl);
                _mm_free(cr);
                _mm_free(cl);

                MFREE(samples);
                n->left = alloc_node();
                RUNP(n->left = bisecting_kmeans(n->left, dm, sl, numseq, num_anchors, num_l,randBuffer));
                n->right = alloc_node();
                RUNP(n->right = bisecting_kmeans(n->right, dm, sr, numseq, num_anchors, num_r,randBuffer));
                return n;
        }

        w = NULL;
        while(1){
                stop++;
                if(stop == 10000){
                        ERROR_MSG("Failed.");
                }
                num_l = 0;
                num_r = 0;

                for(i = 0; i < num_anchors;i++){

                        wr[i] = 0.0f;
                        wl[i] = 0.0f;
                }
                /*fprintf(stdout,"\ncentroids:\n");

                for(j = 0; j < num_anchors;j++){
                        fprintf(stdout,"%f ",cl[j]);

                }
                fprintf(stdout,"\n");
                for(j = 0; j < num_anchors;j++){
                        fprintf(stdout,"%f ",cr[j]);

                }
                fprintf(stdout,"\n");
                fprintf(stdout,"\n");*/
                for(i = 0; i < num_samples;i++){
                        s = samples[i];

                        //edist_serial(dm[s], cl, num_anchors, &dl);
                        //edist_serial(dm[s], cr, num_anchors, &dr);

                        edist_256(dm[s], cl, num_anchors, &dl);
                        edist_256(dm[s], cr, num_anchors, &dr);
                        //fprintf(stdout,"Dist: %f %f\n",dl,dr);
                        if(dr < dl){
                                w = wr;
                                sr[num_r] = s;
                                num_r++;
                        }else{
                                w = wl;
                                sl[num_l] = s;
                                num_l++;
                        }

                        for(j = 0; j < num_anchors;j++){
                                w[j] += dm[s][j];
                        }

                }
                //LOG_MSG("%d %d", num_l,num_r);
                for(j = 0; j < num_anchors;j++){
                        wl[j] /= num_l;

                        wr[j] /= num_r;
                }
                //fprintf(stdout,"\nLeft:\n");

                /*for(j = 0; j < num_anchors;j++){
                        fprintf(stdout,"%f ",wl[j]);

                }
                fprintf(stdout,"\n");
                for(j = 0; j < num_anchors;j++){
                        fprintf(stdout,"%f ",cl[j]);

                }
                fprintf(stdout,"\n");
                fprintf(stdout,"\nRight:\n");
                for(j = 0; j < num_anchors;j++){
                        fprintf(stdout,"%f ",wr[j]);

                }
                fprintf(stdout,"\n");
                for(j = 0; j < num_anchors;j++){
                        fprintf(stdout,"%f ",cr[j]);

                }
                fprintf(stdout,"\n");
                */
                s = 0;
                for(j = 0; j < num_anchors;j++){
                        if(wl[j] != cl[j]){
                                s = 1;
                                break;
                        }
                        if(wr[j] != cr[j]){
                                s = 1;
                                break;

                        }
                }
                if(s){

                        w = cl;
                        cl = wl;
                        wl = w;

                        w = cr;
                        cr = wr;
                        wr = w;
                }else{
                        //LOG_MSG("FINISHED");
                        break;
                }
/* If LLcw  and RRcw, stop. Otherwise, let LLwc:, RRwc: and go back to Step 2 */
        /*         if(wl = cl and wr - cl stop ) */



        /*         else */
        /*                 cl = wl */
        /*                         cr - wr */
        }

        /*fprintf(stdout,"Samples left (%d):\n", num_l);
        for(i = 0; i < num_l;i++){
                fprintf(stdout,"%d ",sl[i]);
        }
        fprintf(stdout,"\n");

        fprintf(stdout,"Samples right (%d):\n", num_r);
        for(i = 0; i < num_r;i++){
                fprintf(stdout,"%d ",sr[i]);
        }
        fprintf(stdout,"\n");*/

        //_mm_free(w);
        _mm_free(wr);
        _mm_free(wl);
        _mm_free(cr);
        _mm_free(cl);
        MFREE(samples);
        n->left = alloc_node();
        RUNP(n->left = bisecting_kmeans(n->left, dm, sl, numseq, num_anchors, num_l,randBuffer));
        n->right = alloc_node();
        RUNP(n->right = bisecting_kmeans(n->right, dm, sr, numseq, num_anchors, num_r,randBuffer));


        return n;
ERROR:
        return NULL;
}




struct node* alloc_node(void)
{
        struct node* n = NULL;
        MMALLOC(n, sizeof(struct node));
        n->left = NULL;
        n->right = NULL;
        n->id = -1;
        return n;
ERROR:
        return NULL;
}

int rec[1000006];

int label_internal(struct node*n, int label)
{
        if(n->left){
                label = label_internal(n->left, label);
        }
        if(n->right){
                label = label_internal(n->right, label);
        }

        if(n->id == -1){
                n->id = label;
                label++;
        }
        return label;

}

int* readbitree(struct node* p,int* tree)
{
        if(p->left){
                tree = readbitree(p->left,tree);
        }
        if(p->right){
                tree = readbitree(p->right,tree);
        }

        if(p->left){
                if(p->right){
                        tree[tree[0]] = p->left->id;
                        tree[tree[0]+1] = p->right->id;
                        tree[tree[0]+2] = p->id;
                        tree[0] +=3;
                        MFREE(p->left);
                        MFREE(p->right);
                }
        }
        return tree;
}


void printTree(struct node* curr,int depth)
{
        int i;
        if(curr==NULL)return;
        printf("\t");
        for(i=0;i<depth;i++){
                if(i==depth-1){
                        printf("%s\u2014\u2014\u2014",rec[depth-1]?"\u0371":"\u221F");
                }else{
                        printf("%s   ",rec[i]?"\u23B8":"  ");
                }
        }
        printf("%d\n",curr->id);
        rec[depth]=1;
        printTree(curr->left,depth+1);
        rec[depth]=0;
        printTree(curr->right,depth+1);
}