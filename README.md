# Very Terrible Mod Loader

Formerly known as the Sonic R Mod Loader. If you're looking for that, use the official Sonic Retro Sonic R Mod Loader at https://github.com/sonicretro/sonicr-mod-loader/releases).

This dumpster-fire is what I spent the last 2 or so years working on. This is very terrible. Please don't use this. Source code is provided for educational purposes. Or maybe you can make it less terrible. It has some neat ideas in it. And it almost sort of kind of works. But really, I replaced this in about 3 hours with a 60 line C program that just abuses the GNU assembler and it completely blows this out of the water. Like, no competition.

## Features

* Runs on Windows NT 4.0! (But not 95, because SQLite doesn't.) 
* Made almost entirely with C
* Compiles with modern compilers *or* compilers from 1999
* Only uses 3 libraries!
* Fully functional Win32 GUI

...can you see why this never got anywhere? I wrote most of this in a Windows 2000 VM because it was the only thing I could run a compiler in that still target Windows 95. My bug tracker was a MS Excel 5.0 (for Windows 3.1!) spreadsheet. For the first year or so all I had was Borland C++ 5.5, which didn't even have a debugger.

Point is, use modern dev software, people. And THEN backport, if you're insane enough to want to do that.

As far as more impressive features go,

* SQLite database for managing mods and patches
* Testing framework compatible with CTest
* CMake-based build system that generates an installer
* Can read mods from ZIP files
* Actually works quite well with simple mods
* Mods can reference subroutines and data from the base game or other mods.
* Can convert from PE addresses to file offsets
* Expression parser, which lets you add or whatever variables, files sizes, checksums, and arbitrary numbers for your starting addresses or whathaveyou.
* Comprehensive manual written in LaTeX.
* Mod generator that converts ASM and C files to mods (in toolkit folder)
* Easy to use Jansson-based wrapper for SQLite functions
* Linux compatible, sort of. (Only the test suite)

There's a bit here that I sort of like, honestly. At minimum it was a really good learning experience in dealing with larger (like more than one file large) projects with build systems and actual architecture and libraries and SQL and ZIP files and user interfaces and stuff. But it's a big bloated monster that fails at it's one job that I'm done dealing with. (The mod generator in the toolkit is actually decent, though. It does it's job.)

## Problems

1. Architecture is kind of crap. It tries. The GUI code is separate and can be swapped out. The SQL database is well-kept. But at the end of the day it's a bunch of confusingly named structs and loose arguments being passed around through so many functions it makes you head spin. If I were to re-write this from scratch, I'd do it more like the ModGenerator in the toolkit folder. Classes. Order. More than 2 structs.

2. Error handling is even more crap. In many cases non-existent. I tried to follow a C standard library-esque global variable to put everything in. However, that lead to some really nasty issues where functions would overwrite an error with an OK value, and the program would keep trucking on into misery. Also like half the program by weight is error handling boilerplate I think. No logging, either, just a chain of never ending and increasingly generic message boxes. If I were to redo this, I'd just write the thing in C++ (or C# or Python or whatever) and use exceptions. If that wasn't an option, I'd pass an ERROR struct around that contains more than a single enum of codes.

3. The SQLite database was completely overkill. Having to constantly go through that slows down the program an incredible amount. Really, I would have been perfectly fine with a list of classes like the ModGenerator has. For cross-struct stuff, I could just code a search function manually. If I were using C#, I could have even set up a LINQ to do that, and it would be so much better.

4. "Whole-file" operators are not implemented. (At least I don't think they are?) This actually would be relatively easy to implement. It's just that everything else was breaking. Horribly.

5. The variable feature is very broken. Namely when it comes to referencing variables from other mods. The idea was that a mod, if it referenced something from another mod, would place an entry in the DB. When a variable got updated by the user or yet another mod (because they were all global and R/W), it would traverse the list and reinstall any patches that used it which would update it to use the new variable value. Problem: dependency chains got quickly monstrous. Infinite loops were not uncommon. It was messy.

6. Over-reliance on allocated strings. It was the best thing I had to interface with the DB and keep UUIDs unique. Early versions were bottlenecked not by patching or anything, but copying strings around. Even worse, these are strings in C. It was arbitrary fixed size or a crap ton of malloc'd buffers. I went for the second, because I thought arbitrary limits were dumb. That was a mistake.

7. Really bad, underspecced initial assumptions that cascaded into a nightmare. When I started, I thought this would be a super easy task. Just a "Oh, the mod will make an entry that says "I'm using this", and then another mod will look and say "okay, can't be here!" and move somewhere else. But then problems cropped up like "How do I keep the mod loader from just plopping stuff in the first byte of the program?" (answer: assume that unless explicitly clear it's used) and "How do I refer to and explicitly clear existing functions in the program?" (answer: make every function it's own space as if a mod installed it) and "what if I need to replace the middle of a function?" (answer: shove a new space type into the mess called SPLIT that splits spaces into 2, and then merge them on uninstallation) and "what if I need to replace a whole function but there's a patch in the middle of it?" (answer: error out because something's probably using that space) and "If a function gets split, what UUID should I give it so that I can still refer to it?" (answer: the same, add a VERSION field to keep everything straight during uninstallation) and basically more of that to infinity and beyond. I didn't know about these problems until they cropped up, and at that point my architecture was so concrete that trying to work around that was getting nigh-on impossible. When variables and reinstallation came into the fray, I basically just gave up, occasionally prodding at it every few months, hoping it would maybe sort of magically work perfectly.

There's obviously more. But I'll stop there. Point is, this was never going to pan out, but I kept going because it constantly seemed almost done. But then more things I realized I needed (like variables, or that expression parser, or whatever) kept popping up, and it was a spiraling tailspin of doom.

Consider this a case study of Not-Invented-Here (because I really should have just taken something off the shelf and modified it), Sunk Cost Fallacy (because I should have stopped even trying like a year ago and tried something else), the importance of using dev tools from the current decade (when I finally got the testing interface running under Linux and Valgrind I was so relieved because I could actually track down errors!) and probably some other stuff.

An incomplete history is in the archive folder. Yes, my version control was a sprawled out mess of folders. Windows 2000 doesn't support Git. You'll notice how I improve the build system and architecture over time from a 6000 line flat file to something with a build system and a test suite. Again, it was a learning process. It just never got anywhere good. You'll also notice how later additions, like the profile selector and the expression parser, seem to be much higher quality than some older code. (Well, okay, they're still crap, but instead of taking 3 weeks to make they took more like 3 days.)

I repeat. Learn from this. It's a valuable educational tool. But don't attempt to actually use this for anything.
