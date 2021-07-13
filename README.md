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

## Putting it all together

This was one of the most confusing thing about multilingual text rendering which is what is the order of each process, does shaping operate on glyphs or chars and does line wrapping run before or after bidi, before or after shaping. It will try and explain how everything fit together to help people better understand what is going on and if some decide to use something that already exists in this area this hopefully will help them understand what that thing does and doesn't do.

Here are the steps:

# 1. The input
The process operate on paragraphs. A paragraph is something that ends with a new line (and usually a period). The new line character doesn't have to be there but if it is there you don't have to remove it before entering the process.

Another input to the process is the paragraph direction. Every editor has a paragraph direction. It is either LTR or RTL. The way you select the direction depends on the context. If you have a UI and the language is of the UI is English then you would set the paragraph direction to LTR. If your UI is Arabic you would set it to RTL. However, there is a third option which is to detect the direction from the paragraph itself. This is useful if your UI has a comment section where people can write anything in their language, in this case, you need to infer the paragraph direction from the comment itself instead of applying the paragraph direction of the interface. The Bidirectional Algorithm has description for the process of detecting the paragraph direction. Checkout the rules [P1, P2, P3](https://www.unicode.org/reports/tr9/#The_Paragraph_Level).

Setting a paragraph direction doesn't mean you can't mix two languages with different directions. It simply means what is the direction of the main language and languages with a different direction than the paragraph direction will be treated as a "quoted" language. To be more specific, it will be used to determine the direction of neutral characters when it is between an RTL char and LTR char.

Note, don't confuse text alignment with paragraph direction. Text alignment doesn't affect layout at all. It is simply whether to align the text to left hand side or the right hand side. Normally, in LTR languages the default text alignment is left and RTL languages it is right. However, you can change it to a different alignment (left, right, or center) without having to layout the paragraph again unlike paragraph direction.

# 2. Processing:

## 1. Apply the rules X1-X10, W1-W7, N0-N2, I1-I2 from the bidirectional algorithm on the paragraph.

The bidirectional algorithm takes as input the paragraph unicode characters and the paragraph direction. After applying the X10 rule you will have and array of runs where each run has a single direction either RTL or LTR (it operates on embedding levels which a little bit more than just a direction). Rules W1-W7, N0-N2, and I1-I2 will operate on those runs and make further changes to the embedding levels. Now, you need to either update the runs according to the modified embedding levels, build them again or you might be able to apply the rules without building the runs and build them once at the end.

At the end of the step, you would have a list of runs (array or linked list) each has one embedding level (or direction) and it must be maximal, meaning you can't have two consecutive runs with the same embedding level. In this case you need to merge them into one.

## 2. Split runs if they contains more than one script.

If a run contains multiple scripts you need to split the run such that it doesn't contain more than one script. A script is writing system used by a language. A script is different from language since multiple languages can use the same script.

For the data and information about this process checkout: [Unicode® Standard Annex #24
UNICODE SCRIPT PROPERTY](http://www.unicode.org/reports/tr24/)

## 3. Split runs further by style and font.

If you have need to change the font face, font style (italic bold), or font size for ranges of text in the paragraph make sure you each run has one set of font properties by splitting runs as necessary.

Even if you don't have style or font changes you need to make sure that the selected font actually support the script in that run and have glyphs for the characters inside. If the selected font doesn't you either have to find a fallback font for the run or ignore it. If you decide to ignore it then you would skip shaping for that run and characters would either appear unshaped or rendered as missing glyphs (outlined rectangles) depending on font.

## 4. For each run apply shaping using the run script shaper.

If the script of a run is a script that requires shaping then apply shaping for that run.

The shaping process takes as input the following:

1. Array of unicode codepoints which is the same as UTF-32 from a single run.

2. CMAP table from TrueType or OpenType font. This table is used to convert Unicode codepoints to glyph IDs. Glyph ID is a number unique within a font used to get the outlines, metrics, or bitmap for that glyph from the font. A glyph ID is specific to the font and it can't be used with other fonts.

3. GSUB and GPOS tables from TrueType or OpenType font. These tables are required by some shapers such as Arabic. It is used to get the glyph for a specific form of a character.

The shaping process will output a list of glyph IDs which can be used for rendering.

For example, the shaper for Arabic script will take a list of Arabic codepoints. It will get the CMAP table from the font. For each codepoint it will decide according to language rules the form that this character need to be in. It will use the CMAP table to convert the codepoints to glyph IDs in the default form then the GSUB table to apply the substitution that will convert the default form to the required form. Arabic also has mandatory ligatures that must be applied. The GSUB table will have substitutions for that to which will replace two or more glyph IDs with one glyph ID for that ligature.

Another example, the shaper for english is simpler. For each codepoint it will use the CMAP to convert the characters to glyph IDs. Optionally, the shaper may apply ligatures substitution if the font provide any.

Other substitutions or typographic features may be provided by the GSUB table and it is up to programmer to either always apply them, ignore them, or provide a settings to enable/disable them.

I will elaborate more on the structure of the GSUB table to help make things clearer. GSUB table is divided into scripts. Each script has a list of features that are identified by a four-letter ID. Shapers will look for the features it wants or requires and use them. For example, in Arabic, each form has feature: 'isol', 'init', 'medi', 'fina'. The Arabic shaper requires those features because without them it will not be able to render the text correctly.
Each feature consist of simple substitution such as replace x with y or more complex substitution such as pattern-match and replace.

You can read more about shaping on MSDN on this [page](https://docs.microsoft.com/en-us/typography/script-development/standard).


## 5. Extract glyphs metrics from the font.

After shaping a run we can get metrics from the font about the text by using glyph IDs. 

## 6. Line wrapping.

After extracting the metrics you can loop over runs in logical order not visual order. Logical order means the order in which unicode characters are layed out in memory before reordering by the bidirectional algorithm (actually reordering is not done by bidi yet). If the run width would fit on the line append the run to the line and move to the next run. If it doesn't fit then try to find break in the run and split the run.

If the line is empty and the run doesn't fit and there isn't any whitespace in the run to break (if it is a long word or a very small line) then you have break the run on any character or append the entire run even if it will overflow the line.

Unfortunately, if you end up splitting the run then you have to shape the splitted runs again.

## 7. Apply the rules L1-L4 from the bidirectional algorithm on each line

These rules will change the behavior of whitespace in certain cases and give you the visual order of the runs for the line in which they are supposed to be rendered.


## 3. Output
At the end of the layout process, we have an list of lines, list of runs for each line, the visual order of runs on the line, the direction of each run, the glyphs IDs and the metrics for each glyph.

This data is enough to render the paragraph, draw selection, draw the cursor and do hit testing.

