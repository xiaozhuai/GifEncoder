/* NeuQuant Neural-Net Quantization Algorithm Interface
 * ----------------------------------------------------
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

#pragma once

#include <cstdio>
#include <cstdint>
#include <atomic>

#define netsize		256		/* number of colours used */

 /* defs for decreasing radius factor */
#define initrad        32	/* for 256 cols, radius starts */

/* For 256 colours, fixed arrays need 8kb, plus space for the image
   ---------------------------------------------------------------- */

   /* four primes near 500 - assume no image has a length so large */
   /* that it is divisible by all four primes */
#define prime1		499
#define prime2		491
#define prime3		487
#define prime4		503

#define minpicturebytes	(3*prime4)		/* minimum size for input image */

public struct NeuQuant {

	/* Types and Global Variables
	   -------------------------- */

	typedef int pixel[4];   /* BGRc */
	pixel network[netsize]; /* the network itself */
	int netindex[256];      /* for network lookup - really 256 */
	int bias[netsize];      /* bias and freq arrays for learning */
	int freq[netsize];
	int radpower[initrad];  /* radpower for precomputation */
	int alphadec;			/* biased by 10 bits */

	//int factor;
	//int samplepixels;

	//int getNetwork(int i, int j);

	/* Initialise network in range (0,0,0) to (255,255,255) and set parameters
	   ----------------------------------------------------------------------- */
	void initnet();

	/* Unbias network to give byte values 0..255 and record position i to prepare for sort
	   ----------------------------------------------------------------------------------- */
	void unbiasnet();	/* can edit this function to do output of colour map */

	/* Output colour map
	   ----------------- */
	   //void writecolourmap(FILE *f); // unused

	void getcolourmap(uint8_t* colorMap);

	/* Insertion sort of network and building of netindex[0..255] (to do after unbias)
	   ------------------------------------------------------------------------------- */
	void inxbuild();

	/* Search for BGR values 0..255 (after net is unbiased) and return colour index
	   ---------------------------------------------------------------------------- */
	int inxsearch(const int b, const int g, const int r);

	/* Main Learning Loop
	 *
	 * @param thepicture - BGR byte pointer array of the picture
	 * @param lengthcount = Width*Height*3
	 * @param samplefac = sampling factor 1..30, 1 is highest quality
	 * @cancelType false - cancel = std::atomic<bool>*, true - cancel = System::Threading::CancellationToken*
	 * @cancel cancellation pointer, send nullptr if you don't have one or don't want it to be cancellable
	   ---------------------------------------------------------------------------------------------------- */
	bool learn(unsigned char* p, const int lengthcount, const int samplefac, int samplepixels, int factor, bool cancelType, void* cancel);

private:
	/* Search for biased BGR values
   ---------------------------- */
	int contest(const int b, const int g, const int r);

	/* Move neuron i towards biased (b,g,r) by factor alpha
	   ---------------------------------------------------- */
	void altersingle(const int alpha, const int i, const int b, const int g, const int r, const int div);

	/* Move adjacent neurons by precomputed alpha*(1-((i-j)^2/[r]^2)) in radpower[|i-j|]
	   --------------------------------------------------------------------------------- */
	void alterneigh(int rad, int i, int b, int g, int r);

};

inline bool isCancel(bool cancelType, void* cancel) {
	return cancel == nullptr ? false : cancelType
		? ((System::Threading::CancellationToken*)cancel)->IsCancellationRequested
		: static_cast<std::atomic<bool>*>(cancel)->load();
}

/* Program Skeleton
   ----------------
  [select samplefac in range 1..30]
  pic = (unsigned char*) malloc(3*width*height);
  [read image from input file into pic]
	initnet();
	learn(pic, 3*width*height,samplefac, width*height/samplefac, <divisor of width*height/samplefac>, false, std::atomic<bool> cancel);
	unbiasnet();
	[write output image header, using writecolourmap(f),
	possibly editing the loops in that function]
	inxbuild();
	[write output image using inxsearch(b,g,r)]		*/
