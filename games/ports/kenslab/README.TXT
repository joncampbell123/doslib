"Ken's Labyrinth" Copyright (c) 1992-1993 Ken Silverman
Ken Silverman's official web site: "http://www.advsys.net/ken"

----------------------------------------------------------------------------

July 1, 2001:

	Some people have been pestering me to release the Ken's Labyrinth source
code, so I've decided that it was time to give it out to everyone. This is
the code from the full version of Ken's Labyrinth (LABFULL.ZIP) It is made
to compile in Microsoft C 6.00A (PLEASE NOTE that this is NOT Microsoft
Visual C++ 6.0, but actually an old 16-bit DOS compiler that was released
by Microsoft way back in 1990.) I have never tried compiling the code in
anything else, and I don't even want to think about it, so please don't ask!

Here are some suggestions if you're interested in fixing up the code:

Major issues:
	- Digitized sound playback crashes the machine. Replace the code!
	- When you hold down the space bar, doors should only swing open and
		  closed once.
	- Movement up & down after using the A&Z keys should stop once you
		  release the keys.

Minor issues:
	- The frame rate would look a lot smoother if you lock it to the vertical
		  refresh interval rather some silly game timer.
	- The texture mapping routines could be re-written.  Right now there are
		  a lot of jagged edges.
	- Masked walls don't use the correct projection. They use some weird
		  kind of geometric interpolation.  I didn't even know it was wrong at
		  the time.
	- Since computers are fast enough, you may as well lock those rotating
		  sprites (fans & warp zones) in their highest detail all the time.

----------------------------------------------------------------------------

KEN'S LABYRINTH SOURCE CODE LICENSE TERMS:                        07/01/2001

[1] I give you permission to make modifications to my Ken's Labyrinth source
		 and distribute it, BUT:

[2] Any derivative works based on my Ken's Labyrinth source may be
		 distributed ONLY through the INTERNET and free of charge - no
		 commercial exploitation whatsoever.

[3] Anything you distribute which uses a part of my Ken's Labyrinth source
		 code MUST include the following message somewhere in the archive:

		 "Ken's Labyrinth" Copyright (c) 1992-1993 Ken Silverman
		 Ken Silverman's official web site: "http://www.advsys.net/ken"

		 Including this README.TXT file along with your distribution is the
		 recommended way of satisfying this requirement.

[4] Technical support: The code is so old that I am NOT interested in wasting
		 my time answering questions about it. If you can't figure out how to
		 compile the code, or you can't figure out what the code is doing,
		 then that's your problem.
