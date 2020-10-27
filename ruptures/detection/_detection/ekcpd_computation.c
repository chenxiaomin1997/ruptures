#include <string.h>
#include <stdlib.h>

#include "kernels.h"

#define min(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

/*
*   res : has to be allocated before the call
*/
void ekcpd_compute(double *signal, int n_samples, int n_dims, int n_bkps, void *kernelDescObj, int **res)
{
    int i, j, t, s, k;
    int c_max_bp;

    /*
    * Allocate memory
    * */
    double *D, *S, *M_V;
    double c_cost, c_cost_sum, c_r;
    int *M_path;

    D = (double *)malloc((n_samples + 1) * sizeof(double));
    S = (double *)malloc((n_samples + 1) * sizeof(double));
    M_V = (double *)malloc((n_bkps + 1) * (n_samples + 1) * sizeof(double));
    M_path = (int *)malloc((n_bkps + 1) * (n_samples + 1) * sizeof(int));

    /*
    * Initialize
    * */
    c_cost = 0;
    c_cost_sum = 0;
    c_r = 0;
    // D and S
    for (i = 0; i < (n_samples + 1); i++)
    {
        D[i] = 0.0;
        S[i] = 0.0;
    }
    // M_V and M_path
    for (i = 0; i < (n_bkps + 1); i++)
    {
        for (j = 0; j < (n_samples + 1); j++)
        {
            M_V[i * (n_samples + 1) + j] = 0.0;
            M_path[i * (n_samples + 1) + j] = 0;
        }
    }

    /*
    * Computation loop
    * */
    // Handle y_{0..t} = {y_0, ..., y_{t-1}}
    for (t = 1; t < (n_samples + 1); t++)
    {
        // Compute D[t], D[t] = D_{0..t}
        D[t] = D[t - 1] + kernel_value_by_name(&(signal[(t - 1) * n_dims]), &(signal[(t - 1) * n_dims]), n_dims, kernelDescObj);

        // Compute S[t-1] = S_{t-1, t}, S[t-2] = S_{t-2, t}, ..., S[0] = S_{0, t}
        // S_{t-1, t} Can be computed with S_{t-1, t-1}.
        // S_{t-1, t-1} was stored in S[t-1]
        // S_{t-1, t} will be stored in S[t-1] as well
        c_r = 0.0;
        for (s = t - 1; s >= 0; s--)
        {
            c_r += kernel_value_by_name(&(signal[s * n_dims]), &(signal[(t - 1) * n_dims]), n_dims, kernelDescObj);
            S[s] += 2 * c_r - kernel_value_by_name(&(signal[(t - 1) * n_dims]), &(signal[(t - 1) * n_dims]), n_dims, kernelDescObj);
        }

        // Compute segmentation
        // Store the total cost on y_{0..t} with 0 break points
        M_V[t] = D[t] - S[0] / t;
        c_max_bp = min(n_bkps, t - 1); // Maximum number of break points on the segment y_{0..t}
        for (k = 1; k <= c_max_bp; k++)
        {
            for (s = k; s <= (t - 1); s++)
            {
                // Compute cost on y_{s..t}
                // D_{s..t} = D_{0..t} - D{0..s} <--> D_{s..t} = D[t] - D[s]
                // S{s..t} has been stored in S[s]
                c_cost = D[t] - D[s] - S[s] / (t - s);
                // With k break points on y_{0..t}, sum cost with (k-1) break points on y_{0..s} and cost on y_{s..t}
                c_cost_sum = M_V[(k - 1) * (n_samples + 1) + s] + c_cost;
                if (s == k)
                {
                    // k is the smallest possibility for s in order to have k break points in y_{0..t}.
                    // It means that y_0, y_1, ..., y_k are break points.
                    M_V[k * (n_samples + 1) + t] = c_cost_sum;
                    M_path[k * (n_samples + 1) + t] = s;
                    continue;
                }
                // Compare to current min
                if (M_V[k * (n_samples + 1) + t] > c_cost_sum)
                {
                    M_V[k * (n_samples + 1) + t] = c_cost_sum;
                    M_path[k * (n_samples + 1) + t] = s;
                }
            }
        }
    }

    /*
    *   Format output
    * */
    k = 1;
    (*res)[n_bkps - 1] = M_path[n_bkps * (n_samples + 1) + n_samples];
    while (k++ < n_bkps)
    {
        (*res)[n_bkps - k] = M_path[(n_bkps - k + 1) * (n_samples + 1) + (*res)[n_bkps - k + 1]];
    }

    /*
    * Free memory
    */
    free(D);
    free(S);
    free(M_V);
    free(M_path);

    return;
}