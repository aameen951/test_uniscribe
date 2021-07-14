# Multilingual Text Rendering

This code includes as complete text layout and rendering for multilingual paragraphs as possible using Windows Uniscribe API.


## Building The Code
You need visual studio 2019 (or later?) to be installed with the Desktop Development tools packages.

You must have the compiler in the path to be able to build the code or you can use the x64 Native Tools Command Prompt which already have the compiler in the path.

You can build the code by simply running the `cc.bat` script.

## Running The Code
The executable will be in the `build` directory inside the project folder. 

You can also run the application by running the script `run.bat`.

The program will render test paragraphs that are baked inside the source code. You can change them by modifying the source code and building it again.

You can use the `Up` and `Down` arrow keys to scroll vertically.
You can use the `Left` and `Right` arrow keys to change the maximum width used for laying out the paragraph to experiment with line-wrapping.

# What is going on with Multilingual Text Rendering?!

To start with, I don't think I'm qualified to write about this. However, due to the small number of resources and documentation about this subject (I don't know of any resources that cover everything), I'm just going to write what I know about this subject. Feel free to correct me by opening an issue.

Multilingual Text Rendering is complicated for three reasons:
1. Lack of resources, as pointed out.
2. Most people don't know more than one or two languages and one of them is usually English (which is one of the easiest languages).
3. Knowing about all the edge cases and testing them.

There is nothing inherently complicated about this problem or the algorithms involved. It is just the lack of knowledge about other languages, the features they have and all the edge cases that are not thought about.

My first language is Arabic which is one of the most complicated languages (if not the most) because it is written from right to left, characters are rendered differently depending on their position inside the word, and it has diacritics (which are small marks that are placed on top/bottom of previous characters).

Since I like text rendering and my first language is Arabic, that is what motivated me to look into this subject. Again, I'm not an expert in this subject, I learned through digging into it to try to implement a text renderer from scratch, and every time I face a problem that I didn't know about. I still didn't implement a complete multilingual text renderer from scratch and before that, I wouldn't call myself an expert.

## Concepts in Text Rendering

### 1. Bidirectional Algorithm (Bidi for short)
Not all languages are written from left to right (LTR), some languages are written from right to left (RTL) such as (Arabic, Urdu, Hebrew).

This requires a process to determine how to render a paragraph that mixes between two languages. An example of this is the following:

| (1) | (2) | (3) |
|:-:|:-:|:-:|
| He says: " | السـلام عليكم | ". |
| `--------->` | `<----------` | `->` |

You might think that this would only be a problem if you mix two languages but that is not correct because numbers in RTL languages are still written left to right:

| (3) | (2) | (1) |
|:-:|:-:|:-:|
| عاماً | 35 | عمره |
| `<---`  | `-->` | `<---` |

The Bidirectional Algorithm was developed by Unicode to process a paragraph and split it into parts called runs where each run contains characters with one direction. It tells you what the direction of each run is and the order in which these runs should be rendered.

The bidi algorithm actually does the following:
1. Split a paragraph into runs.
2. Assign classes to each Unicode characters (LTR, RTL, Neutral, Weak, ...).
3. Handle neutral characters and assign them a direction based on the context.
4. Handle brackets and tell you if they need to be mirrored.
5. Provide Unicode characters to override the direction of characters.
6. Provide Unicode characters to isolate part of the text and prevent it from reordering the surrounding text.
7. ...

You can learn more about it here: [Unicode® Standard Annex #9 UNICODE BIDIRECTIONAL ALGORITHM](https://unicode.org/reports/tr9/).

## 2. Shaping
You might be familiar with a similar concept in English called ligatures. Some fonts include custom glyphs for certain letters when they come after each other. For example, some fonts have a specific glyph for the letters `fi` where they are merged together. This behavior is specified by the font using something called a substitution. TrueType and OpenType fonts have a table called GSUB which includes rules that tell the rendering engine if a specified pattern matches then substitute multiple glyphs with one glyph or one glyph with multiple glyphs.

Shaping is more complicated than that. Different scripts have different rules for choosing what the character should look like when rendered based on its position in the word and the surrounding characters. The process that implements these rules is called shaper. Each run has a script determined from the characters inside it. Based on that script the rendering engine will choose the shaper that will be applied on that run. The term script is more accurate than language in this context because multiple languages might use the same script. The shaper will determine the form of each character according to the rules of that script. Then, it will get the substitution for that form from the GSUB table to substitute the glyph of the default form with the glyph of the required form.

I'll show examples of this in details for Arabic to make sure the concept is clear. However, different scripts have different rules.

In Arabic, letters connect with their neighbors. Some letters connect with its neighbors from both sides, some letters connect from one side, and some connect with neither sides.

The following table show some examples :

| Type | Isolated | Initial | Medial | Final |
|-|:-:|:-:|:-:|:-:|
| Both Sides| ب | بـ | ـبـ | ـب |
| Both Sides | ح | حـ | ـحـ | ـح |
| One Side | ر | - | - | ـر |
| Neither | ء | - | - | - |

If a letter can connect from the right side and its neighbor on the right can connect from the left side then the appropriate form will be selected for each one so they can connect with each others as follows:

| Can connect<br> from right | Can connect<br> from both | Result |
|:-:|:-:|:-:|
| ر | ب | ب ر
| ـر | بـ | بــر

If a letter can connect from both sides and its neighbor on the right can connect from the left side and its neighbor on the left can connect from the right side then the appropriate form will be selected for each one so they can connect with each others as follows:

|Can connect<br> from right | Can connect<br> from both | Can connect<br> from both | Result |
|:-:|:-:|:-:|:-:|
|ر | ب | ح | ح ب ر
|ـر | ـبـ | حـ | حــبــر

That's actually shaping for Arabic. It is not that complicated. It's mostly about knowing this information, verifying that the output is correct, getting the right substitutions from the font and applying it correctly.

Similarly, diacritics in Arabic have their rules, and their positioning information are extracted from GPOS table inside TrueType or OpenType font.

I will skip the examples for diacritics for now. If I study other scripts later I might add a separate section where I document shaping in details for each script and I will add examples for diacritics in details there. If you are reading this and would like to contribute information about your language please open an issue.

MSDN has more information about shaping for each script [here](https://docs.microsoft.com/en-us/typography/script-development/standard).

### 3. Line Wrapping
Line wrapping is very easy for LTR languages where mostly each character corresponds to one glyph, you just add the glyph widths and apply kerning if it is available and wrap to the next line when line width exceeds the limit.

However, when working with bidirectional text and complex shaping, it is more involved. Line wrapping requires metrics to break long paragraphs. Metrics requires glyphs which means shaping and bidi must be applied first on the paragraph.

During line wrapping you might need to split runs into multiple run to fit it on the line. This means that shaping must be done again on the split runs.

Although the bidi algorithm is applied at the start it is not done; the bidi algorithm includes processes that need to operate on lines (after wrapping is done) to give you the order in which the runs will appear on the line and it will reset the direction of whitespace at the end of the line to paragraph direction.

There are also recommendations for finding line break opportunities by [Unicode® Standard Annex #14 UNICODE LINE BREAKING ALGORITHM](https://unicode.org/reports/tr14/) if you want to check it out.

## Putting it all together

This was one of the most confusing things about multilingual text rendering. When does each process start? When does it finish? Does shaping operate on glyphs or chars? Is line wrapping applied before or after bidi, before or after shaping? 

I will try to explain how everything fits together to help people better understand what is going on and if someone decides to use a library, this hopefully will help them know what it does and doesn't do. Here are the steps:

## 1. The input
The process operate on paragraphs. A paragraph is something that ends with a new line. The new line character doesn't have to be there but if it is there you don't have to remove it before entering the process.

Another input to the process is the paragraph direction. Every editor has a paragraph direction. It is either LTR or RTL. The way you select the direction depends on the context. If you have a UI and the language is English, you set the paragraph direction to LTR. If your UI is Arabic, you set it to RTL. There is a third option which is to detect the direction from the paragraph itself. This is useful if your UI has a comment section where people, potentially, can write in their language; in this case, you need to infer the paragraph direction from the comment itself instead of applying the paragraph direction of the interface. The Bidirectional Algorithm has a description for the process of detecting the paragraph direction. Check out the rules [P1, P2, P3](https://www.unicode.org/reports/tr9/#The_Paragraph_Level).

Specifying a paragraph direction doesn't mean you can't mix two languages with different directions. It is specifying the direction of the main language. Languages with a different direction in this paragraph will be treated as hosted languages. For example, it will be used to determine the direction of neutral characters when it is between an RTL char and LTR char.

Note, don't confuse text alignment with paragraph direction. Text alignment doesn't affect layout at all. It is simply whether to align the text to left hand side or the right hand side. Normally, in LTR languages the default text alignment is left and in RTL languages it is right. However, you can change it to a different alignment (left, right, or center) without having to lay out the paragraph again unlike paragraph direction.

## 2. Processing:

### 1. Apply the rules X1-X10, W1-W7, N0-N2, I1-I2 from the bidirectional algorithm on the paragraph.

The bidirectional algorithm takes as input the paragraph codepoints (characters encoded in UTF-32) and the paragraph direction and apply the rules on those codepoints.

After applying the rule X10 you will have and array of runs where each run has a single direction either RTL or LTR (more accurately, each run will have a single embedding level which is a little bit more than just the direction). 

Rules W1-W7, N0-N2, and I1-I2 will operate on those runs and make further changes to the embedding levels. After applying them, you either need to update the runs according to the modified embedding levels, build them again or you might be able to apply the rules without building the runs and build them once at the end.

At the end of the step, you would have a list of runs (array or linked list) each has one embedding level (or direction) and it is maximal, meaning there isn't two consecutive runs with the same embedding level.

The bidi algorithm requires some of the data provided by the [Unicode Character Database (UCD)](https://unicode.org/ucd/). Specifically, it requires the BidiClass from [DerivedBidiClass.txt](https://www.unicode.org/Public/UCD/latest/ucd/extracted/DerivedBidiClass.txt) and Bidi_Paired_Bracket_Type from [BidiBrackets.txt](https://www.unicode.org/Public/UCD/latest/ucd/BidiBrackets.txt). The data assigns properties to codepoints that the bidi algorithm requires to operate.

### 2. Split runs if they contains more than one script.

If a run contains multiple scripts you need to split the run such that it doesn't contain more than one script. A script is a writing system used by a language. A script is different from language since multiple languages can use the same script.

For the data and information about this process checkout: [Unicode® Standard Annex #24 UNICODE SCRIPT PROPERTY](http://www.unicode.org/reports/tr24/)

### 3. Split runs further by style and font.

If you need to change the font face, font style (italic, bold), or font size for ranges of text in the paragraph make sure each run has one set of font properties by splitting runs as necessary.

Even if you don't have style or font changes you need to make sure that the selected font supports the script in that run and has glyphs for the codepoints in the run. If the selected font doesn't then you either have to find a fallback font for the run or ignore it. If you decided to ignore it then you would skip shaping for that run, and codepoints would either appear unshaped or rendered as missing glyphs (outlined rectangles) depending on the font.

### 4. For each run apply shaping using the run script shaper.

If the script of a run is a script that requires shaping then apply shaping for that run.

The shaping process takes as input the following:

1. Array of unicode codepoints from a single run.

2. CMAP table from TrueType or OpenType font. This table is used to convert Unicode codepoints to glyph IDs. Glyph ID is a number unique within a font used to get the outlines, metrics, or bitmap for that glyph from the font. A glyph ID is specific to the font and it can't be used with other fonts.

3. GSUB and GPOS tables from TrueType or OpenType font. These tables are required by complex shapers such as Arabic shaper. It is used to get the glyph for a specific form of a codepoint.

The shaping process will output a list of glyph IDs which can be used for rendering.

For example, the shaper for Arabic script will take a list of Arabic codepoints. It will get the CMAP table from the font. For each codepoint it will decide according to language rules what form the character should to be in. It will use the CMAP table to convert the codepoints to glyph IDs in the default form then the GSUB table to apply the substitution that will convert the default form to the required form. Arabic also has mandatory ligatures that must be applied. The GSUB table will have substitutions for that which will replace two or more glyph IDs with one glyph ID.

Another example, the Latin shaper, is simpler. For each codepoint it will use the CMAP to convert the characters to glyph IDs. Optionally, the shaper may apply ligatures substitution if the font provides any.

Other substitutions or typographic features may be provided by the GSUB table and it is up to programmer to either always apply them, ignore them, or provide user settings to enable/disable them.

I will elaborate more on the structure of the GSUB table to help make things clearer. GSUB table is divided into scripts. Each script has a list of features that are identified by a four-letter ID. Shapers will look for the features it wants and use them. For example, in Arabic, each form has a feature: 'isol' for the isolated form , 'init' for the initial, 'medi' for the medial form, and 'fina' for the final form. The Arabic shaper requires those features because without them it will not be able to render the text correctly.
Each feature is a list of simple substitutions such as replace x with y or more complex substitutions such as pattern-match and replace.

MSDN has more information about shaping for each script [here](https://docs.microsoft.com/en-us/typography/script-development/standard).

### 5. Extract glyphs metrics from the font.

After shaping a run, we can get metrics from the font about the text by using glyph IDs. 

### 6. Line wrapping.

After extracting the metrics you can loop over runs in logical order not visual order. Logical order means the order in which codepoints are layed out in memory before reordering by the bidirectional algorithm (actually reordering is not done by bidi yet). If the run fits on the line append it to the line and move to the next run. If it doesn't fit then try to find break in the run and split the run.

If the line is empty, the run doesn't fit and there isn't any whitespace in the run to break (if it is a long word or a very small line) then you have break the run on any codepoint or append the entire run even if it will overflow the line otherwise, the process will not terminate and it will loop indefinitely.

Unfortunately, if you end up splitting the run then you have to shape the splitted runs again and check the run width again.

There are also recommendations for finding line break opportunities by [Unicode® Standard Annex #14 UNICODE LINE BREAKING ALGORITHM](https://unicode.org/reports/tr14/) if you want to check it out.

### 7. Apply the rules L1-L4 from the bidirectional algorithm on each line

These rules are applied to lines after wrapping. They change the behavior of whitespace in certain cases such as the end of the line and before tabs and they give you the visual order of the runs for the line in which they are supposed to be rendered.

## 3. Output

At the end of the layout process, you will have an list of lines, list of runs for each line, the visual order of runs on the line, the direction of each run (RTL or LTR), the glyphs IDs and the metrics for each glyph.

This data is enough to render the paragraph, draw selection, draw the cursor and do hit testing.

# Conclusion

# What does Uniscribe API provide?

    TODO

You read the source code and comments included to learn more about Uniscribe API, what it does and how it works. Check out the file `layout.cpp`.

# What does DirectWrite API provide?

    TODO

# What does HarfBuzz provide?

    TODO

