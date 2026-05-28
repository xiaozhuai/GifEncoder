/* NeuQuant Neural-Net Quantization Algorithm
 * ------------------------------------------
 *
 * Copyright (c) 1994 Anthony Dekker
 *
 * NEUQUANT Neural-Net quantization algorithm by Anthony Dekker, 1994.
 * See "Kohonen neural networks for optimal colour quantization"
 * in "Network: Computation in Neural Systems" Vol. 5 (1994) pp 351-367.
 * for a discussion of the algorithm.
 * See also  http://members.ozemail.com.au/~dekker/NEUQUANT.HTML
 *
 * Any party obtaining a copy of these files from the author, directly or
 * indirectly, is granted, free of charge, a full and unrestricted irrevocable,
 * world-wide, paid up, royalty-free, nonexclusive right and license to deal
 * in this software and documentation files (the "Software"), including without
 * limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons who receive
 * copies from any such party to do so, with the only requirement being
 * that this copyright notice remain intact.
 */


#include "NeuQuant.h"

 /* Network Definitions
------------------- */

#define maxnetpos    255
#define netbiasshift    4            /* bias for colour values */
#define ncycles        100            /* no. of learning cycles */

/* defs for freq and bias */
#define intbiasshift    16            /* bias for fractions */
#define intbias        65536
#define gammashift    10            /* gamma = 1024 */
#define gamma    1024
#define betashift    10
#define beta        64    /* beta = 1/1024 */
#define betagamma    65536

/* defs for decreasing radius factor */
#define radiusbiasshift    6            /* at 32.0 biased by 6 bits */
#define radiusbias    64
#define initradius    2048    /* and decreases by a */
#define radiusdec    30            /* factor of 1/30 each cycle */

/* defs for decreasing alpha factor */
#define alphabiasshift    10            /* alpha starts at 1.0 */
#define initalpha    1024


/* radbias and alpharadbias used for radpower calculation */
#define radbiasshift    8
#define radbias        256
#define alpharadbshift  18
#define alpharadbias    262144


/*int getNetwork(int i, int j) {
    return network[i][j];
}*/

/* Initialise network in range (0,0,0) to (255,255,255) and set parameters
   ----------------------------------------------------------------------- */

void NeuQuant::initnet() {
    for (int* p, i = 0; i < netsize; i++) {
        p = network[i];
        p[0] = p[1] = p[2] = (i << (netbiasshift + 8)) / netsize;
        freq[i] = intbias / netsize;    // 1/netsize
        bias[i] = 0;
    }
}


/* Unbias network to give byte values 0..255 and record position i to prepare for sort
   ----------------------------------------------------------------------------------- */

void NeuQuant::unbiasnet() {
    int i, j, temp;
    for (i = 0; i < netsize; i++) {
        auto& ni = network[i];
        for (j = 0; j < 3; j++) // OLD CODE: network[i][j] >>= netbiasshift; Fix based on bug report by Juergen Weigert jw@suse.de
            ni[j] = (temp = (network[i][j] + (1 << (netbiasshift - 1))) >> netbiasshift) > 255 ? 255 : temp;
        ni[3] = i;            // record colour no
    }
}


/* Output colour map
   ----------------- */

   /*void writecolourmap(FILE* f) {
       int i, j;

       for (i = 2; i >= 0; i--)
           for (j = 0; j < netsize; j++)
               putc(network[j][i], f);
   }*/

void NeuQuant::getcolourmap(uint8_t* colorMap) {
    int* index = new int[netsize];
    for (int i = 0; i < netsize; i++)
        index[network[i][3]] = i;
    int k = 0;
    for (int i = 0; i < netsize; i++) {
        auto& nj = network[index[i]];
        colorMap[k++] = nj[2];
        colorMap[k++] = nj[1];
        colorMap[k++] = nj[0];
    }
    delete[] index;
}


/* Insertion sort of network and building of netindex[0..255] (to do after unbias)
   ------------------------------------------------------------------------------- */

void NeuQuant::inxbuild() {

    int i, j, smallpos, smallval;
    int* p, * q;
    int previouscol, startpos;

    previouscol = 0;
    startpos = 0;
    for (i = 0; i < netsize; i++) {
        p = network[i];
        smallpos = i;
        smallval = p[1];            // index on g
        // find smallest in i..netsize-1
        for (j = i + 1; j < netsize; j++) {
            q = network[j];
            if (q[1] < smallval) {        // index on g 
                smallpos = j;
                smallval = q[1];    // index on g
            }
        }
        q = network[smallpos];
        // swap p (i) and q (smallpos) entries
        if (i != smallpos) {
            j = q[0];
            q[0] = p[0];
            p[0] = j;
            j = q[1];
            q[1] = p[1];
            p[1] = j;
            j = q[2];
            q[2] = p[2];
            p[2] = j;
            j = q[3];
            q[3] = p[3];
            p[3] = j;
        }
        // smallval entry is now in position i 
        if (smallval != previouscol) {
            netindex[previouscol] = (startpos + i) >> 1;
            for (j = previouscol + 1; j < smallval; j++) netindex[j] = i;
            previouscol = smallval;
            startpos = i;
        }
    }
    netindex[previouscol] = (startpos + maxnetpos) >> 1;
    for (j = previouscol + 1; j < 256; j++) netindex[j] = maxnetpos; // really 256 
}


/* Search for BGR values 0..255 (after net is unbiased) and return colour index
   ---------------------------------------------------------------------------- */

int NeuQuant::inxsearch(const int b, const int g, const int r) {
    int i = netindex[g], // index on g
        j = i - 1,  // start at netindex[g] and work outwards
        * p, dist, a, best = -1,
        bestd = 1000; // biggest possible dist is 256*3
    while (i < netsize || j >= 0) {
        if (i < netsize) {
            if ((dist = (p = network[i])[1] - g) < bestd) { // inx key
                ++i;
                if (dist < 0)
                    dist = -dist;
                if (((a = p[0] - b) < 0 ? dist -= a : dist += a) < bestd && ((a = p[2] - r) < 0 ? dist -= a : dist += a) < bestd) {
                    bestd = dist;
                    best = p[3];
                }
            } else i = netsize;    // stop iter 
        }
        if (j >= 0) {
            if ((dist = g - (p = network[j])[1]) < bestd) { // inx key - reverse dif
                --j;
                if (dist < 0)
                    dist = -dist;
                if (((a = p[0] - b) < 0 ? dist -= a : dist += a) < bestd && ((a = p[2] - r) < 0 ? dist -= a : dist += a) < bestd) {
                    bestd = dist;
                    best = p[3];
                }
            } else j = -1; // stop iter
        }
    }
    return best;
}


/* Search for biased BGR values
   ---------------------------- */

int NeuQuant::contest(const int b, const int g, const int r) {
    /* finds closest neuron (min dist) and updates freq */
    /* finds best neuron (min dist-bias) and returns position */
    /* for frequently chosen neurons, freq[i] is high and bias[i] is negative */
    /* bias[i] = gamma*((1/netsize)-freq[i]) */

    int i, dist, a, biasdist, betafreq;
    int bestpos = -1, bestbiaspos = -1, bestd = ~(1 << 31), bestbiasd = ~(1 << 31);
    int* p = bias, * f = freq, * n;

    for (i = 0; i < netsize; i++) {
        if ((dist = (n = network[i])[0] - b) < 0)
            dist = -dist;
        dist += (a = n[1] - g) < 0 ? -a : a;
        if (((a = n[2] - r) < 0 ? dist -= a : dist += a) < bestd) {
            bestd = dist;
            bestpos = i;
        }
        if ((biasdist = dist - ((*p) >> (intbiasshift - netbiasshift))) < bestbiasd) {
            bestbiasd = biasdist;
            bestbiaspos = i;
        }
        betafreq = (*f >> betashift);
        *f++ -= betafreq;
        *p++ += (betafreq << gammashift);
    }
    freq[bestpos] += beta;
    bias[bestpos] -= betagamma;
    return bestbiaspos;
}

/* Move neuron i towards biased (b,g,r) by factor alpha
   ---------------------------------------------------- */

void NeuQuant::altersingle(const int alpha, const int i, const int b, const int g, const int r, const int div) {

    int* n = network[i];                /* alter hit neuron */
    *n -= (alpha * (*n - b)) / div;
    n++;
    *n -= (alpha * (*n - g)) / div;
    n++;
    *n -= (alpha * (*n - r)) / div;
}


/* Move adjacent neurons by precomputed alpha*(1-((i-j)^2/[r]^2)) in radpower[|i-j|]
   --------------------------------------------------------------------------------- */

void NeuQuant::alterneigh(int rad, int i, int b, int g, int r) {
    int j = i + 1, k = i - 1, lo = i - rad, hi = i + rad, a;
    int* q = radpower;
    if (lo < -1)
        lo = -1;
    if (hi > netsize)
        hi = netsize;
    while (j < hi || k > lo) {
        a = *++q;
        if (j < hi)
            altersingle(a, j++, b, g, r, alpharadbias);
        if (k > lo)
            altersingle(a, k--, b, g, r, alpharadbias);
    }
}

/* Main Learning Loop
   ------------------ */

bool NeuQuant::learn(unsigned char* p, const int lengthcount, const int samplefac, int samplepixels, int factor, bool cancelType, void* cancel) {
    int i, j, b, g, r;
    int radius = initradius, rad = radius >> radiusbiasshift, alpha, step, delta = samplepixels / ncycles;
    //const unsigned char *p = thepicture;
    const unsigned char* lim = p + lengthcount;

    alphadec = 30 + ((samplefac - 1) / 3);
    if (delta <= 0)
        delta = 1; // i had to add this fix to stop it from crashing with frames that are so small that samplepixels < ncycles
    alpha = initalpha;
    if (rad <= 1)
        rad = 0;
    int sqrrad = rad * rad;
    for (i = 0; i < rad; i++)
        radpower[i] = alpha * (((sqrrad - i * i) * radbias) / sqrrad);
    step = (lengthcount % prime1 != 0 ? prime1 : lengthcount % prime2 != 0 ? prime2 : lengthcount % prime3 != 0 ? prime3 : prime4) * 3;
    i = 0;

    if (cancel == nullptr) {
        while (0 <= --samplepixels) {
            b = p[0] << netbiasshift;
            g = p[1] << netbiasshift;
            r = p[2] << netbiasshift;
            j = contest(b, g, r);
            altersingle(alpha, j, b, g, r, initalpha);
            if (rad)
                alterneigh(rad, j, b, g, r);   // alter neighbours
            if ((p += step) >= lim)
                p -= lengthcount;
            if ((++i) % delta == 0) {
                alpha -= alpha / alphadec;
                if ((rad = (radius -= radius / radiusdec) >> radiusbiasshift) <= 1)
                    rad = 0;
                for (j = 0; j < rad; j++)
                    radpower[j] = alpha * (((rad * rad - j * j) * radbias) / (rad * rad));
            }
        }
        return false;
    }
    //auto& token = *canceltoken;

    samplepixels /= factor;
    while (0 <= --factor) {
        if (isCancel(cancelType, cancel))
            return true;
        for (int s = samplepixels; 0 <= --s;) {
            b = p[0] << netbiasshift;
            g = p[1] << netbiasshift;
            r = p[2] << netbiasshift;
            j = contest(b, g, r);
            altersingle(alpha, j, b, g, r, initalpha);
            if (rad)
                alterneigh(rad, j, b, g, r);   // alter neighbours
            if ((p += step) >= lim)
                p -= lengthcount;
            if ((++i) % delta == 0) {
                alpha -= alpha / alphadec;
                if ((rad = (radius -= radius / radiusdec) >> radiusbiasshift) <= 1)
                    rad = 0;
                for (j = 0; j < rad; j++)
                    radpower[j] = alpha * (((rad * rad - j * j) * radbias) / (rad * rad));
            }
        }
    }
    return false;
}
