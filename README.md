# Multilingual Text Rendering

This code includes "mostly" complete text layout and rendering for multilingual paragraphs
using Windows Uniscribe API.


## Building The Code
You need visual studio 2019 (or later?) to be installed with the Desktop Development
tools packages.

You must have the compiler in the path to be able to build the code or you can use
the x64 Native Tools Command Prompt which already have the compiler in the path.

You can build the code by simply running the `cc.bat` script.

## Running The Code
The executable will be in the `build` directory inside the project folder. 

You can also run the application by running the script `run.bat`.

The program will render test paragraphs that are baked inside the source code.
You can change them by modifying the source code and building it again.

You can use the `Up` and `Down` arrow keys to scroll vertically.
You can use the `Left` and `Right` arrow keys to change the maximum width used for laying
out the paragraph to experiment with line wrapping.

# What is going on with Multilingual Text Rendering?!

To start with, I don't think I'm qualified to write about this. However, due to the few and 
limited amount of resources and documents about this subject (actually I don't know any 
resource about this) I'm just going to write what I know about this subject and feel free
to correct me by opening an issue.

Multilingual Text Rendering is complicated for three reasons:
1. Lack of resources, as pointed out.
2. Most people don't know more than one or two languages and one of them is usually English (which is the easiest language).
3. Knowing about all the edge cases and testing them.

There is nothing inherently complicated about this problem or the algorithms involved. It is just the lack of knowledge about other languages and the features they have and all the edge cases that are not thought about.

My first language is Arabic which is one of the most complicated languages (if not the most) because it is an Right-to-Left language, characters are rendered differently depending on their position inside the word, and it has diacritics (which are small marks that are placed on top/bottom of previous characters).

Since I like text rendering and my first language is Arabic, that is what motivated me to look into this subject. Again, I'm not an expert in this subject, I learned through digging
into it trying to implement a text renderer from scratch and every time I face problem
that I didn't know about and I try again and fail and try again. I still didn't implement 
a complete multilingual text renderer from scratch and before that I wouldn't call my self an expert.

## Concepts in Text Rendering

### 1. Bidirectional Algorithm (Bidi for short)
Not all languages are written from left to right (LTR), some languages are written from 
right to left (RTL) such as (Arabic, Urdu, Hebrew).

This requires a process to determine how to render a paragraph that mixes between two languages. An example of this is the following:

    He says: "  السلام عليكم  ".
    --------->  <-----------  ->
       (1)           (2)      (3)

You might think that this would only be a problem if you mix two languages but that is not 
correct because numbers in RTL languages are still written left to right:


    عمره  35  عاماً
    <---  -->  <---
     (3)  (2)  (1)

The Bidirectional Algorithm was developed by Unicode to process a paragraph and split it into
parts called runs where each run contains characters with one direction. It tells you
what direction of each run is and the order in which these runs should be rendered.

The bidi algorithm actually does the following:
1. Split a paragraph into runs.
2. Assign classes to each Unicode characters (LTR, RTL, Neutral, Weak, ...).
3. Handle neutral characters and assign them a direction based on the context.
4. Handle brackets and mirror them if they need to.
5. Provide Unicode characters to override the direction of characters.
6. Provide Unicode characters to isolate part of the text and prevent it from reordering the surrounding text.

You can learn more about it here: [Unicode Standard Annex #9 Unicode Bidirectional Algorithm](https://unicode.org/reports/tr9/).

## 2. Shaping
You might be familiar with a similar concept in English called ligatures. Some fonts include
custom glyph for a certain letters if they come after each other. For example, some font
has a specific glyph for the letter `f` followed by the letter `i` where they are merged together. This is behavior is specified by the font using something called substitution.
TrueType and OpenType fonts have a table called GSUB which includes rules that tells the 
rendering engine if a specified pattern matches substitute multiple glyphs with one glyph, 
one glyph with multiple glyphs, etc.

Shaping is more complicated than that. The rendering engine have scanners (called Shapers)
one for each language (script is the accurate term in this context). Depending on the script
of the run, the rendering engine will apply the shaper on that run and determine the form
of each character according to the rules of the script. After determining the form, it will
get the substitution for that form from the GSUB table to substitute the character with the 
glyph that corresponds to required form.

I'll show examples of this in details for Arabic to make sure the concept is clear. However,
each script has different rules.

In Arabic, letters connect with neighbors. Some letters connect with neighbors from both
sides, some letters connect from one side, and some connect with neither sides.

The following table show some examples :

| Type | Isolated | Initial | Medial | Final |
|-|:-:|:-:|:-:|:-:|
| Both Sides| ب | بـ | ـبـ | ـب |
| Both Sides | ح | حـ | ـحـ | ـح |
| One Side | ر | - | - | ـر |
| Neither | ء | - | - | - |

If a letter can connect from the right side and its neighbor on the right can connect with
its left then the appropriate form will be selected for each one so they can connect with 
each others as follows:

| Can connect<br> from Right | Can connect<br> from both | Result |
|:-:|:-:|:-:|
| ر | ب | ب ر
| ـر | بـ | بــر

If a letter can connect from both sides and its neighbor on the right can connect with
its left and its neighbor on the left can connect with its right then the appropriate form will be selected for each one so they can connect with each others as follows:

|Can connect<br> from Right | Can connect<br> from both | Can connect<br> from Both | Result |
|:-:|:-:|:-:|:-:|
|ر | ب | ح | ح ب ر
|ـر | ـبـ | حـ | حــبــر

That's actually shaping for Arabic. It is not that complicated. It's mostly about knowing
this information, verifying that the output is correct, getting the right substitutions from
the font and applying it correctly.

Similarly, diacritics in Arabic have their rules and their positioning information are extracted from GPOS table inside TrueType or OpenType font.

I will skip the examples for diacritics for now. If I study other scripts in details later
I might add a separate section where I document shaping in details for each script and I will add examples for diacritics in details there. If you are reading this and would like to contribute information about your language please open an issue.

### 3. Line Wrapping
Line wrapping is very easy for LTR languages where mostly each character correspond to one glyph, you just add the glyph widths and kerning if it is available and wrap to the next line
when line size exceeds the limit.

However, when working with bidirectional text and complex shaping it more involved. Line wrapping requires metrics to break long paragraphs. Metrics requires glyphs which means shaping and bidi must already be applied on the paragraph.

During line wrapping you might need to split runs into multiple run to fit it on the line. 
This means that shaping must be done again on the split runs.

Although the bidi algorithm is applied at the beginning it is not actually done. The bidi
algorithm includes processes that need to operate on lines (after wrapping is done) to
give you the order in which the runs will appear on the line and .


## Putting it All Together
