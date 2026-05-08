// MIT License
//
// Copyright (c) 2026 Code Life
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

////////////////////////////////////////////////////////////////////////////////
// bv.cpp
// Bible verse lookup tool -- outputs Bible references to stdout in plain text.

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <regex>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#define popen  _popen
#define pclose _pclose
#else
#include <termios.h>
#include <unistd.h>
#endif

using namespace std;

#ifdef _WIN32
#define HOME_ENV "USERPROFILE"
#else
#define HOME_ENV "HOME"
#endif

const string VERSION = "1.21";
const string CONFIG_FILE = ".verselumen";
const string SECTION     = "bv";
// Reading plans
static const char* const PLAN_CHRONOLOGICAL[365] = {
    "Genesis 1-3",                                   // 1
    "Genesis 4-7",                                   // 2
    "Genesis 8-11",                                  // 3
    "Job 1-5",                                       // 4
    "Job 6-9",                                       // 5
    "Job 10-13",                                     // 6
    "Job 14-16",                                     // 7
    "Job 17-20",                                     // 8
    "Job 21-23",                                     // 9
    "Job 24-28",                                     // 10
    "Job 29-31",                                     // 11
    "Job 32-34",                                     // 12
    "Job 35-37",                                     // 13
    "Job 38-39",                                     // 14
    "Job 40-42",                                     // 15
    "Genesis 12-15",                                 // 16
    "Genesis 16-18",                                 // 17
    "Genesis 19-21",                                 // 18
    "Genesis 22-24",                                 // 19
    "Genesis 25-26",                                 // 20
    "Genesis 27-29",                                 // 21
    "Genesis 30-31",                                 // 22
    "Genesis 32-34",                                 // 23
    "Genesis 35-37",                                 // 24
    "Genesis 38-40",                                 // 25
    "Genesis 41-42",                                 // 26
    "Genesis 43-45",                                 // 27
    "Genesis 46-47",                                 // 28
    "Genesis 48-50",                                 // 29
    "Exodus 1-3",                                    // 30
    "Exodus 4-6",                                    // 31
    "Exodus 7-9",                                    // 32
    "Exodus 10-12",                                  // 33
    "Exodus 13-15",                                  // 34
    "Exodus 16-18",                                  // 35
    "Exodus 19-21",                                  // 36
    "Exodus 22-24",                                  // 37
    "Exodus 25-27",                                  // 38
    "Exodus 28-29",                                  // 39
    "Exodus 30-32",                                  // 40
    "Exodus 33-35",                                  // 41
    "Exodus 36-38",                                  // 42
    "Exodus 39-40",                                  // 43
    "Leviticus 1-4",                                 // 44
    "Leviticus 5-7",                                 // 45
    "Leviticus 8-10",                                // 46
    "Leviticus 11-13",                               // 47
    "Leviticus 14-15",                               // 48
    "Leviticus 16-18",                               // 49
    "Leviticus 19-21",                               // 50
    "Leviticus 22-23",                               // 51
    "Leviticus 24-25",                               // 52
    "Leviticus 26-27",                               // 53
    "Numbers 1-2",                                   // 54
    "Numbers 3-4",                                   // 55
    "Numbers 5-6",                                   // 56
    "Numbers 7",                                     // 57
    "Numbers 8-10",                                  // 58
    "Numbers 11-13",                                 // 59
    "Numbers 14-15",                                 // 60
    "Numbers 16-17",                                 // 61
    "Numbers 18-20",                                 // 62
    "Numbers 21-22",                                 // 63
    "Numbers 23-25",                                 // 64
    "Numbers 26-27",                                 // 65
    "Numbers 28-30",                                 // 66
    "Numbers 31-32",                                 // 67
    "Numbers 33-34",                                 // 68
    "Numbers 35-36",                                 // 69
    "Deuteronomy 1-2",                               // 70
    "Deuteronomy 3-4",                               // 71
    "Deuteronomy 5-7",                               // 72
    "Deuteronomy 8-10",                              // 73
    "Deuteronomy 11-13",                             // 74
    "Deuteronomy 14-16",                             // 75
    "Deuteronomy 17-20",                             // 76
    "Deuteronomy 21-23",                             // 77
    "Deuteronomy 24-27",                             // 78
    "Deuteronomy 28-29",                             // 79
    "Deuteronomy 30-31",                             // 80
    "Deuteronomy 32-34; Psalm 90",                   // 81
    "Joshua 1-4",                                    // 82
    "Joshua 5-8",                                    // 83
    "Joshua 9-11",                                   // 84
    "Joshua 12-15",                                  // 85
    "Joshua 16-18",                                  // 86
    "Joshua 19-21",                                  // 87
    "Joshua 22-24",                                  // 88
    "Judges 1-2",                                    // 89
    "Judges 3-5",                                    // 90
    "Judges 6-7",                                    // 91
    "Judges 8-9",                                    // 92
    "Judges 10-12",                                  // 93
    "Judges 13-15",                                  // 94
    "Judges 16-18",                                  // 95
    "Judges 19-21",                                  // 96
    "Ruth",                                          // 97
    "1 Samuel 1-3",                                  // 98
    "1 Samuel 4-8",                                  // 99
    "1 Samuel 9-12",                                 // 100
    "1 Samuel 13-14",                                // 101
    "1 Samuel 15-17",                                // 102
    "1 Samuel 18-20; Psalms 11, 59",                 // 103
    "1 Samuel 21-24; Psalm 91",                      // 104
    "Psalms 7, 27, 31, 34, 52",                      // 105
    "Psalms 56, 120, 140-142",                       // 106
    "1 Samuel 25-27",                                // 107
    "Psalms 17, 35, 54, 63",                         // 108
    "1 Samuel 28-31; Psalm 18",                      // 109
    "Psalms 121, 123-125, 128-130",                  // 110
    "2 Samuel 1-4",                                  // 111
    "Psalms 6, 8-10, 14, 16, 19, 21",               // 112
    "1 Chronicles 1-2",                              // 113
    "Psalms 43-45, 49, 84-85, 87",                  // 114
    "1 Chronicles 3-5",                              // 115
    "Psalms 73, 77-78",                              // 116
    "1 Chronicles 6",                                // 117
    "Psalms 81, 88, 92-93",                          // 118
    "1 Chronicles 7-10",                             // 119
    "Psalms 102-104",                                // 120
    "2 Samuel 5; 1 Chronicles 11-12",                // 121
    "Psalm 133",                                     // 122
    "Psalms 106-107",                                // 123
    "1 Chronicles 13-16",                            // 124
    "Psalms 1-2, 15, 22-24, 47, 68",                // 125
    "Psalms 89, 96, 100-101, 105, 132",              // 126
    "2 Samuel 6-7; 1 Chronicles 17",                 // 127
    "Psalms 25, 29, 33, 36, 39",                     // 128
    "2 Samuel 8-9; 1 Chronicles 18",                 // 129
    "Psalms 50, 53, 60, 75",                         // 130
    "2 Samuel 10; 1 Chronicles 19; Psalm 20",        // 131
    "Psalms 65-67, 69-70",                           // 132
    "2 Samuel 11-12; 1 Chronicles 20",               // 133
    "Psalms 32, 51, 86, 122",                        // 134
    "2 Samuel 13-15",                                // 135
    "Psalms 3-4, 12-13, 28, 55",                    // 136
    "2 Samuel 16-18",                                // 137
    "Psalms 26, 40, 58, 61-62, 64",                 // 138
    "2 Samuel 19-21",                                // 139
    "Psalms 5, 38, 41-42",                           // 140
    "2 Samuel 22-23; Psalm 57",                      // 141
    "Psalms 95, 97-99",                              // 142
    "2 Samuel 24; 1 Chronicles 21-22; Psalm 30",     // 143
    "Psalms 108-110",                                // 144
    "1 Chronicles 23-25",                            // 145
    "Psalms 131, 138-139, 143-145",                  // 146
    "1 Chronicles 26-29; Psalm 127",                 // 147
    "Psalms 111-118",                                // 148
    "1 Kings 1-2; Psalms 37, 71, 94",               // 149
    "Psalm 119",                                     // 150
    "1 Kings 3-4",                                   // 151
    "2 Chronicles 1; Psalm 72",                      // 152
    "Song of Solomon",                               // 153
    "Proverbs 1-3",                                  // 154
    "Proverbs 4-6",                                  // 155
    "Proverbs 7-9",                                  // 156
    "Proverbs 10-12",                                // 157
    "Proverbs 13-15",                                // 158
    "Proverbs 16-18",                                // 159
    "Proverbs 19-21",                                // 160
    "Proverbs 22-24",                                // 161
    "1 Kings 5-6; 2 Chronicles 2-3",                 // 162
    "1 Kings 7; 2 Chronicles 4",                     // 163
    "1 Kings 8; 2 Chronicles 5",                     // 164
    "2 Chronicles 6-7; Psalm 136",                   // 165
    "Psalms 134, 146-150",                           // 166
    "1 Kings 9; 2 Chronicles 8",                     // 167
    "Proverbs 25-26",                                // 168
    "Proverbs 27-29",                                // 169
    "Ecclesiastes 1-6",                              // 170
    "Ecclesiastes 7-12",                             // 171
    "1 Kings 10-11; 2 Chronicles 9",                 // 172
    "Proverbs 30-31",                                // 173
    "1 Kings 12-14",                                 // 174
    "2 Chronicles 10-12",                            // 175
    "1 Kings 15; 2 Chronicles 13-16",                // 176
    "1 Kings 16; 2 Chronicles 17",                   // 177
    "1 Kings 17-19",                                 // 178
    "1 Kings 20-21",                                 // 179
    "1 Kings 22; 2 Chronicles 18",                   // 180
    "2 Chronicles 19-23",                            // 181
    "Obadiah; Psalms 82-83",                         // 182
    "2 Kings 1-4",                                   // 183
    "2 Kings 5-8",                                   // 184
    "2 Kings 9-11",                                  // 185
    "2 Kings 12-13; 2 Chronicles 24",                // 186
    "2 Kings 14; 2 Chronicles 25",                   // 187
    "Jonah",                                         // 188
    "2 Kings 15; 2 Chronicles 26",                   // 189
    "Isaiah 1-4",                                    // 190
    "Isaiah 5-8",                                    // 191
    "Amos 1-5",                                      // 192
    "Amos 6-9",                                      // 193
    "2 Chronicles 27; Isaiah 9-12",                  // 194
    "Micah",                                         // 195
    "2 Chronicles 28; 2 Kings 16-17",                // 196
    "Isaiah 13-17",                                  // 197
    "Isaiah 18-22",                                  // 198
    "Isaiah 23-27",                                  // 199
    "2 Kings 18; 2 Chronicles 29-31; Psalm 48",      // 200
    "Hosea 1-7",                                     // 201
    "Hosea 8-14",                                    // 202
    "Isaiah 28-30",                                  // 203
    "Isaiah 31-34",                                  // 204
    "Isaiah 35-36",                                  // 205
    "Isaiah 37-39; Psalm 76",                        // 206
    "Isaiah 40-43",                                  // 207
    "Isaiah 44-48",                                  // 208
    "2 Kings 19; Psalms 46, 80, 135",                // 209
    "Isaiah 49-53",                                  // 210
    "Isaiah 54-58",                                  // 211
    "Isaiah 59-63",                                  // 212
    "Isaiah 64-66",                                  // 213
    "2 Kings 20-21",                                 // 214
    "2 Chronicles 32-33",                            // 215
    "Nahum",                                         // 216
    "2 Kings 22-23; 2 Chronicles 34-35",             // 217
    "Zephaniah",                                     // 218
    "Jeremiah 1-3",                                  // 219
    "Jeremiah 4-6",                                  // 220
    "Jeremiah 7-9",                                  // 221
    "Jeremiah 10-13",                                // 222
    "Jeremiah 14-17",                                // 223
    "Jeremiah 18-22",                                // 224
    "Jeremiah 23-25",                                // 225
    "Jeremiah 26-29",                                // 226
    "Jeremiah 30-31",                                // 227
    "Jeremiah 32-34",                                // 228
    "Jeremiah 35-37",                                // 229
    "Jeremiah 38-40; Psalms 74, 79",                 // 230
    "2 Kings 24-25; 2 Chronicles 36",                // 231
    "Habakkuk",                                      // 232
    "Jeremiah 41-45",                                // 233
    "Jeremiah 46-48",                                // 234
    "Jeremiah 49-50",                                // 235
    "Jeremiah 51-52",                                // 236
    "Lamentations 1-2",                              // 237
    "Lamentations 3-5",                              // 238
    "Ezekiel 1-4",                                   // 239
    "Ezekiel 5-8",                                   // 240
    "Ezekiel 9-12",                                  // 241
    "Ezekiel 13-15",                                 // 242
    "Ezekiel 16-17",                                 // 243
    "Ezekiel 18-20",                                 // 244
    "Ezekiel 21-22",                                 // 245
    "Ezekiel 23-24",                                 // 246
    "Ezekiel 25-27",                                 // 247
    "Ezekiel 28-30",                                 // 248
    "Ezekiel 31-33",                                 // 249
    "Ezekiel 34-36",                                 // 250
    "Ezekiel 37-39",                                 // 251
    "Ezekiel 40-42",                                 // 252
    "Ezekiel 43-45",                                 // 253
    "Ezekiel 46-48",                                 // 254
    "Joel",                                          // 255
    "Daniel 1-3",                                    // 256
    "Daniel 4-6",                                    // 257
    "Daniel 7-9",                                    // 258
    "Daniel 10-12",                                  // 259
    "Ezra 1-3",                                      // 260
    "Ezra 4-6; Psalm 137",                           // 261
    "Haggai",                                        // 262
    "Zechariah 1-4",                                 // 263
    "Zechariah 5-9",                                 // 264
    "Zechariah 10-14",                               // 265
    "Esther 1-5",                                    // 266
    "Esther 6-10",                                   // 267
    "Ezra 7-10",                                     // 268
    "Nehemiah 1-5",                                  // 269
    "Nehemiah 6-7",                                  // 270
    "Nehemiah 8-10",                                 // 271
    "Nehemiah 11-13; Psalm 126",                     // 272
    "Malachi",                                       // 273
    "Luke 1; John 1",                                // 274
    "Matthew 1; Luke 2",                             // 275
    "Matthew 2",                                     // 276
    "Matthew 3; Mark 1; Luke 3",                     // 277
    "Matthew 4; Luke 4-5",                           // 278
    "John 2-4",                                      // 279
    "Matthew 8; Mark 2",                             // 280
    "John 5",                                        // 281
    "Matthew 12; Mark 3; Luke 6",                    // 282
    "Matthew 5-7",                                   // 283
    "Matthew 9; Luke 7",                             // 284
    "Matthew 11",                                    // 285
    "Luke 11",                                       // 286
    "Matthew 13; Luke 8",                            // 287
    "Mark 4-5",                                      // 288
    "Matthew 10",                                    // 289
    "Matthew 14; Mark 6; Luke 9",                    // 290
    "John 6",                                        // 291
    "Matthew 15; Mark 7",                            // 292
    "Matthew 16; Mark 8",                            // 293
    "Matthew 17; Mark 9",                            // 294
    "Matthew 18",                                    // 295
    "John 7-8",                                      // 296
    "John 9-10",                                     // 297
    "Luke 10",                                       // 298
    "Luke 12-13",                                    // 299
    "Luke 14-15",                                    // 300
    "Luke 16-17",                                    // 301
    "John 11",                                       // 302
    "Luke 18",                                       // 303
    "Matthew 19; Mark 10",                           // 304
    "Matthew 20-21",                                 // 305
    "Luke 19",                                       // 306
    "Mark 11; John 12",                              // 307
    "Matthew 22; Mark 12",                           // 308
    "Matthew 23; Luke 20-21",                        // 309
    "Mark 13",                                       // 310
    "Matthew 24",                                    // 311
    "Matthew 25",                                    // 312
    "Matthew 26; Mark 14",                           // 313
    "Luke 22; John 13",                              // 314
    "John 14-17",                                    // 315
    "Matthew 27; Mark 15",                           // 316
    "Luke 23; John 18-19",                           // 317
    "Matthew 28; Mark 16",                           // 318
    "Luke 24; John 20-21",                           // 319
    "Acts 1-3",                                      // 320
    "Acts 4-6",                                      // 321
    "Acts 7-8",                                      // 322
    "Acts 9-10",                                     // 323
    "Acts 11-12",                                    // 324
    "Acts 13-14",                                    // 325
    "James",                                         // 326
    "Acts 15-16",                                    // 327
    "Galatians 1-3",                                 // 328
    "Galatians 4-6",                                 // 329
    "Acts 17",                                       // 330
    "1 & 2 Thessalonians",                           // 331
    "Acts 18-19",                                    // 332
    "1 Corinthians 1-4",                             // 333
    "1 Corinthians 5-8",                             // 334
    "1 Corinthians 9-11",                            // 335
    "1 Corinthians 12-14",                           // 336
    "1 Corinthians 15-16",                           // 337
    "2 Corinthians 1-4",                             // 338
    "2 Corinthians 5-9",                             // 339
    "2 Corinthians 10-13",                           // 340
    "Romans 1-3",                                    // 341
    "Romans 4-7",                                    // 342
    "Romans 8-10",                                   // 343
    "Romans 11-13",                                  // 344
    "Romans 14-16",                                  // 345
    "Acts 20-23",                                    // 346
    "Acts 24-26",                                    // 347
    "Acts 27-28",                                    // 348
    "Colossians, Philemon",                          // 349
    "Ephesians",                                     // 350
    "Philippians",                                   // 351
    "1 Timothy",                                     // 352
    "Titus",                                         // 353
    "1 Peter",                                       // 354
    "Hebrews 1-6",                                   // 355
    "Hebrews 7-10",                                  // 356
    "Hebrews 11-13",                                 // 357
    "2 Timothy",                                     // 358
    "2 Peter, Jude",                                 // 359
    "1 John",                                        // 360
    "2, 3 John",                                     // 361
    "Revelation 1-5",                                // 362
    "Revelation 6-11",                               // 363
    "Revelation 12-18",                              // 364
    "Revelation 19-22",                              // 365
};

static const char* const PLAN_OTNT[365] = {
    "Genesis 1-3; Matthew 1",                                       // 1
    "Genesis 4-6; Matthew 2",                                       // 2
    "Genesis 7-9; Matthew 3",                                       // 3
    "Genesis 10-12; Matthew 4",                                     // 4
    "Genesis 13-15; Matthew 5:1-26",                                // 5
    "Genesis 16-17; Matthew 5:27-48",                               // 6
    "Genesis 18-19; Matthew 6:1-18",                                // 7
    "Genesis 20-22; Matthew 6:19-34",                               // 8
    "Job 1-2; Matthew 7",                                           // 9
    "Job 3-4; Matthew 8:1-17",                                      // 10
    "Job 5-7; Matthew 8:18-34",                                     // 11
    "Job 8-10; Matthew 9:1-17",                                     // 12
    "Job 11-13; Matthew 9:18-38",                                   // 13
    "Job 14-16; Matthew 10:1-20",                                   // 14
    "Job 17-19; Matthew 10:21-42",                                  // 15
    "Job 20-21; Matthew 11",                                        // 16
    "Job 22-24; Matthew 12:1-23",                                   // 17
    "Job 25-27; Matthew 12:24-50",                                  // 18
    "Job 28-29; Matthew 13:1-30",                                   // 19
    "Job 30-31; Matthew 13:31-58",                                  // 20
    "Job 32-33; Matthew 14:1-21",                                   // 21
    "Job 34-35; Matthew 14:22-36",                                  // 22
    "Job 36-37; Matthew 15:1-20",                                   // 23
    "Job 38-40; Matthew 15:21-39",                                  // 24
    "Job 41-42; Matthew 16",                                        // 25
    "Genesis 23-24; Matthew 17",                                    // 26
    "Genesis 25-26; Matthew 18:1-20",                               // 27
    "Genesis 27-28; Matthew 18:21-35",                              // 28
    "Genesis 29-30; Matthew 19",                                    // 29
    "Genesis 31-32; Matthew 20:1-16",                               // 30
    "Genesis 33-35; Matthew 20:17-34",                              // 31
    "Genesis 36-38; Matthew 21:1-22",                               // 32
    "Genesis 39-40; Matthew 21:23-46",                              // 33
    "Genesis 41-42; Matthew 22:1-22",                               // 34
    "Genesis 43-45; Matthew 22:23-46",                              // 35
    "Genesis 46-48; Matthew 23:1-22",                               // 36
    "Genesis 49-50; Matthew 23:23-39",                              // 37
    "Exodus 1-3; Matthew 24:1-28",                                  // 38
    "Exodus 4-6; Matthew 24:29-51",                                 // 39
    "Exodus 7-8; Matthew 25:1-30",                                  // 40
    "Exodus 9-11; Matthew 25:31-46",                                // 41
    "Exodus 12-13; Matthew 26:1-35",                                // 42
    "Exodus 14-15; Matthew 26:36-75",                               // 43
    "Exodus 16-18; Matthew 27:1-26",                                // 44
    "Exodus 19-20; Matthew 27:27-50",                               // 45
    "Exodus 21-22; Matthew 27:51-66",                               // 46
    "Exodus 23-24; Matthew 28",                                     // 47
    "Exodus 25-26; Mark 1:1-22",                                    // 48
    "Exodus 27-28; Mark 1:23-45",                                   // 49
    "Exodus 29-30; Mark 2",                                         // 50
    "Exodus 31-33; Mark 3:1-19",                                    // 51
    "Exodus 34-35; Mark 3:20-35",                                   // 52
    "Exodus 36-38; Mark 4:1-20",                                    // 53
    "Exodus 39-40; Mark 4:21-41",                                   // 54
    "Psalm 90; Leviticus 1-2; Mark 5:1-20",                         // 55
    "Leviticus 3-5; Mark 5:21-43",                                  // 56
    "Leviticus 6-7; Mark 6:1-29",                                   // 57
    "Leviticus 8-10; Mark 6:30-56",                                 // 58
    "Leviticus 11-12; Mark 7:1-13",                                 // 59
    "Leviticus 13; Mark 7:14-37",                                   // 60
    "Leviticus 14; Mark 8:1-21",                                    // 61
    "Leviticus 15-16; Mark 8:22-38",                                // 62
    "Leviticus 17-18; Mark 9:1-29",                                 // 63
    "Leviticus 19-20; Mark 9:30-50",                                // 64
    "Leviticus 21-22; Mark 10:1-31",                                // 65
    "Leviticus 23-24; Mark 10:32-52",                               // 66
    "Leviticus 25; Mark 11:1-18",                                   // 67
    "Leviticus 26-27; Mark 11:19-33",                               // 68
    "Numbers 1-2; Mark 12:1-27",                                    // 69
    "Numbers 3-4; Mark 12:28-44",                                   // 70
    "Numbers 5-6; Mark 13:1-20",                                    // 71
    "Numbers 7-8; Mark 13:21-37",                                   // 72
    "Numbers 9-11; Mark 14:1-26",                                   // 73
    "Numbers 12-14; Mark 14:27-53",                                 // 74
    "Numbers 15-16; Mark 14:54-72",                                 // 75
    "Numbers 17-19; Mark 15:1-25",                                  // 76
    "Numbers 20-22; Mark 15:26-47",                                 // 77
    "Numbers 23-25; Mark 16",                                       // 78
    "Numbers 26-27; Luke 1:1-20",                                   // 79
    "Numbers 28-30; Luke 1:21-38",                                  // 80
    "Numbers 31-33; Luke 1:39-56",                                  // 81
    "Numbers 34-36; Luke 1:57-80",                                  // 82
    "Deuteronomy 1-2; Luke 2:1-24",                                 // 83
    "Deuteronomy 3-4; Luke 2:25-52",                                // 84
    "Deuteronomy 5-7; Luke 3",                                      // 85
    "Deuteronomy 8-10; Luke 4:1-30",                                // 86
    "Deuteronomy 11-13; Luke 4:31-44",                              // 87
    "Deuteronomy 14-16; Luke 5:1-16",                               // 88
    "Deuteronomy 17-19; Luke 5:17-39",                              // 89
    "Deuteronomy 20-22; Luke 6:1-26",                               // 90
    "Deuteronomy 23-25; Luke 6:27-49",                              // 91
    "Deuteronomy 26-27; Luke 7:1-30",                               // 92
    "Deuteronomy 28-29; Luke 7:31-50",                              // 93
    "Deuteronomy 30-31; Luke 8:1-25",                               // 94
    "Deuteronomy 32-34; Luke 8:26-56",                              // 95
    "Joshua 1-3; Luke 9:1-17",                                      // 96
    "Joshua 4-6; Luke 9:18-36",                                     // 97
    "Joshua 7-9; Luke 9:37-62",                                     // 98
    "Joshua 10-12; Luke 10:1-24",                                   // 99
    "Joshua 13-15; Luke 10:25-42",                                  // 100
    "Joshua 16-18; Luke 11:1-28",                                   // 101
    "Joshua 19-21; Luke 11:29-54",                                  // 102
    "Joshua 22-24; Luke 12:1-31",                                   // 103
    "Judges 1-3; Luke 12:32-59",                                    // 104
    "Judges 4-6; Luke 13:1-22",                                     // 105
    "Judges 7-8; Luke 13:23-35",                                    // 106
    "Judges 9-10; Luke 14:1-24",                                    // 107
    "Judges 11-12; Luke 14:25-35",                                  // 108
    "Judges 13-15; Luke 15:1-10",                                   // 109
    "Judges 16-18; Luke 15:11-32",                                  // 110
    "Judges 19-21; Luke 16",                                        // 111
    "Ruth 1-4; Luke 17:1-19",                                       // 112
    "1 Samuel 1-3; Luke 17:20-37",                                  // 113
    "1 Samuel 4-6; Luke 18:1-23",                                   // 114
    "1 Samuel 7-9; Luke 18:24-43",                                  // 115
    "1 Samuel 10-12; Luke 19:1-27",                                 // 116
    "1 Samuel 13-14; Luke 19:28-48",                                // 117
    "1 Samuel 15-16; Luke 20:1-26",                                 // 118
    "1 Samuel 17-18; Luke 20:27-47",                                // 119
    "1 Samuel 19; Psalm 23, 59; Luke 21:1-19",                      // 120
    "1 Samuel 20-21; Psalm 34; Luke 21:20-38",                      // 121
    "1 Samuel 22; Psalm 56; Luke 22:1-23",                          // 122
    "Psalm 52, 57, 142; Luke 22:24-46",                             // 123
    "1 Samuel 23; Psalm 54, 63; Luke 22:47-71",                     // 124
    "1 Samuel 24-27; Luke 23:1-25",                                 // 125
    "1 Samuel 28-29; Luke 23:26-56",                                // 126
    "1 Samuel 30-31; Luke 24:1-35",                                 // 127
    "2 Samuel 1-2; Luke 24:36-53",                                  // 128
    "2 Samuel 3-5; John 1:1-28",                                    // 129
    "2 Samuel 6-7; Psalm 30; John 1:29-51",                         // 130
    "2 Samuel 8-9; Psalm 60; John 2",                               // 131
    "2 Samuel 10-12; John 3:1-15",                                  // 132
    "Psalm 32, 51; John 3:16-36",                                   // 133
    "2 Samuel 13-14; John 4:1-26",                                  // 134
    "2 Samuel 15; Psalm 3, 69; John 4:27-54",                       // 135
    "2 Samuel 16-18; John 5:1-24",                                  // 136
    "2 Samuel 19-20; John 5:25-47",                                 // 137
    "Psalm 64, 70; John 6:1-21",                                    // 138
    "2 Samuel 21-22; Psalm 18; John 6:22-40",                       // 139
    "2 Samuel 23-24; John 6:41-71",                                 // 140
    "Psalm 4-6; John 7:1-27",                                       // 141
    "Psalm 7-8; John 7:28-53",                                      // 142
    "Psalm 9, 11; John 8:1-27",                                     // 143
    "Psalm 12-14; John 8:28-59",                                    // 144
    "Psalm 15-16; John 9:1-23",                                     // 145
    "Psalm 17, 19; John 9:24-41",                                   // 146
    "Psalm 20-22; John 10:1-21",                                    // 147
    "Psalm 24-26; John 10:22-42",                                   // 148
    "Psalm 27-29; John 11:1-29",                                    // 149
    "Psalm 31, 35; John 11:30-57",                                  // 150
    "Psalm 36-38; John 12:1-26",                                    // 151
    "Psalm 39-41; John 12:27-50",                                   // 152
    "Psalm 53, 55, 58; John 13:1-20",                               // 153
    "Psalm 61-62, 65; John 13:21-38",                               // 154
    "Psalm 68, 72, 86; John 14",                                    // 155
    "Psalm 101, 103, 108; John 15",                                  // 156
    "Psalm 109-110, 138; John 16",                                   // 157
    "Psalm 139-141; John 17",                                       // 158
    "Psalm 143-145; John 18:1-18",                                  // 159
    "1 Kings 1-2; John 18:19-40",                                   // 160
    "1 Kings 3-4; Proverbs 1; John 19:1-22",                        // 161
    "Proverbs 2-4; John 19:23-42",                                  // 162
    "Proverbs 5-7; John 20",                                        // 163
    "Proverbs 8-9; John 21",                                        // 164
    "Proverbs 10-12; Acts 1",                                       // 165
    "Proverbs 13-15; Acts 2:1-21",                                  // 166
    "Proverbs 16-18; Acts 2:22-47",                                 // 167
    "Proverbs 19-21; Acts 3",                                       // 168
    "Proverbs 22-24; Acts 4:1-22",                                  // 169
    "Proverbs 25-26; Acts 4:23-37",                                 // 170
    "Proverbs 27-29; Acts 5:1-21",                                  // 171
    "Proverbs 30-31; Acts 5:22-42",                                 // 172
    "Song of Solomon 1-3; Acts 6",                                  // 173
    "Song of Solomon 4-5; Acts 7:1-21",                             // 174
    "Song of Solomon 6-8; Acts 7:22-43",                            // 175
    "1 Kings 5-7; Acts 7:44-60",                                    // 176
    "1 Kings 8-9; Acts 8:1-25",                                     // 177
    "1 Kings 10-11; Acts 8:26-40",                                  // 178
    "Ecclesiastes 1-3; Acts 9:1-22",                                // 179
    "Ecclesiastes 4-6; Acts 9:23-43",                               // 180
    "Ecclesiastes 7-9; Acts 10:1-23",                               // 181
    "Ecclesiastes 10-12; Acts 10:24-48",                            // 182
    "1 Kings 12-13; Acts 11",                                       // 183
    "1 Kings 14-15; Acts 12",                                       // 184
    "1 Kings 16-18; Acts 13:1-25",                                  // 185
    "1 Kings 19-20; Acts 13:26-52",                                 // 186
    "1 Kings 21-22; Acts 14",                                       // 187
    "2 Kings 1-3; James 1",                                         // 188
    "2 Kings 4-6; James 2",                                         // 189
    "2 Kings 7-9; James 3",                                         // 190
    "2 Kings 10-12; James 4",                                       // 191
    "2 Kings 13-14; James 5",                                       // 192
    "Jonah 1-4; Acts 15:1-21",                                      // 193
    "Amos 1-3; Acts 15:22-41",                                      // 194
    "Amos 4-6; Galatians 1",                                        // 195
    "Amos 7-9; Galatians 2",                                        // 196
    "2 Kings 15-16; Galatians 3",                                   // 197
    "2 Kings 17-18; Galatians 4",                                   // 198
    "2 Kings 19-21; Galatians 5",                                   // 199
    "2 Kings 22-23; Galatians 6",                                   // 200
    "2 Kings 24-25; Acts 16:1-21",                                  // 201
    "Psalm 1-2, 10; Acts 16:22-40",                                 // 202
    "Psalm 33, 43, 66; Philippians 1",                              // 203
    "Psalm 67, 71; Philippians 2",                                  // 204
    "Psalm 89, 92; Philippians 3",                                  // 205
    "Psalm 93-95; Philippians 4",                                   // 206
    "Psalm 96-98; Acts 17:1-15",                                    // 207
    "Psalm 99-100, 102; Acts 17:16-34",                             // 208
    "Psalm 104-105; 1 Thessalonians 1",                             // 209
    "Psalm 106, 111-112; 1 Thessalonians 2",                        // 210
    "Psalm 113-115; 1 Thessalonians 3",                             // 211
    "Psalm 116-118; 1 Thessalonians 4",                             // 212
    "Psalm 119:1-88; 1 Thessalonians 5",                            // 213
    "Psalm 119:89-176; 2 Thessalonians 1",                          // 214
    "Psalm 120-122; 2 Thessalonians 2",                             // 215
    "Psalm 123-125; 2 Thessalonians 3",                             // 216
    "Psalm 127-129; Acts 18",                                       // 217
    "Psalm 130-132; 1 Corinthians 1",                               // 218
    "Psalm 133-135; 1 Corinthians 2",                               // 219
    "Psalm 136, 146; 1 Corinthians 3",                              // 220
    "Psalm 147-148; 1 Corinthians 4",                               // 221
    "Psalm 149-150; 1 Corinthians 5",                               // 222
    "1 Chronicles 1-3; 1 Corinthians 6",                            // 223
    "1 Chronicles 4-6; 1 Corinthians 7:1-19",                       // 224
    "1 Chronicles 7-9; 1 Corinthians 7:20-40",                      // 225
    "1 Chronicles 10-12; 1 Corinthians 8",                          // 226
    "1 Chronicles 13-15; 1 Corinthians 9",                          // 227
    "1 Chronicles 16; Psalm 42, 44; 1 Corinthians 10:1-18",         // 228
    "Psalm 45-47; 1 Corinthians 10:19-33",                          // 229
    "Psalm 48-50; 1 Corinthians 11:1-16",                           // 230
    "Psalm 73, 85; 1 Corinthians 11:17-34",                         // 231
    "Psalm 87-88; 1 Corinthians 12",                                // 232
    "1 Chronicles 17-19; 1 Corinthians 13",                         // 233
    "1 Chronicles 20-22; 1 Corinthians 14:1-20",                    // 234
    "1 Chronicles 23-25; 1 Corinthians 14:21-40",                   // 235
    "1 Chronicles 26-27; 1 Corinthians 15:1-28",                    // 236
    "1 Chronicles 28-29; 1 Corinthians 15:29-58",                   // 237
    "2 Chronicles 1-3; 1 Corinthians 16",                           // 238
    "2 Chronicles 4-6; 2 Corinthians 1",                            // 239
    "2 Chronicles 7-9; 2 Corinthians 2",                            // 240
    "2 Chronicles 10-12; 2 Corinthians 3",                          // 241
    "2 Chronicles 13-14; 2 Corinthians 4",                          // 242
    "2 Chronicles 15-16; 2 Corinthians 5",                          // 243
    "2 Chronicles 17-18; 2 Corinthians 6",                          // 244
    "2 Chronicles 19-20; 2 Corinthians 7",                          // 245
    "2 Chronicles 21; Obadiah 1; 2 Corinthians 8",                  // 246
    "2 Chronicles 22; Joel 1; 2 Corinthians 9",                     // 247
    "2 Chronicles 23; Joel 2-3; 2 Corinthians 10",                  // 248
    "2 Chronicles 24-26; 2 Corinthians 11:1-15",                    // 249
    "Isaiah 1-2; 2 Corinthians 11:16-33",                           // 250
    "Isaiah 3-4; 2 Corinthians 12",                                 // 251
    "Isaiah 5-6; 2 Corinthians 13",                                 // 252
    "2 Chronicles 27-28; Acts 19:1-20",                             // 253
    "2 Chronicles 29-30; Acts 19:21-41",                            // 254
    "2 Chronicles 31-32; Acts 20:1-16",                             // 255
    "Isaiah 7-8; Acts 20:17-38",                                    // 256
    "Isaiah 9-10; Ephesians 1",                                     // 257
    "Isaiah 11-13; Ephesians 2",                                    // 258
    "Isaiah 14-16; Ephesians 3",                                    // 259
    "Isaiah 17-19; Ephesians 4",                                    // 260
    "Isaiah 20-22; Ephesians 5:1-16",                               // 261
    "Isaiah 23-25; Ephesians 5:17-23",                              // 262
    "Isaiah 26-27; Ephesians 6",                                    // 263
    "Isaiah 28-29; Romans 1",                                       // 264
    "Isaiah 30-31; Romans 2",                                       // 265
    "Isaiah 32-33; Romans 3",                                       // 266
    "Isaiah 34-36; Romans 4",                                       // 267
    "Isaiah 37-38; Romans 5",                                       // 268
    "Isaiah 39-40; Romans 6",                                       // 269
    "Isaiah 41-42; Romans 7",                                       // 270
    "Isaiah 43-44; Romans 8:1-21",                                  // 271
    "Isaiah 45-46; Romans 8:22-39",                                 // 272
    "Isaiah 47-49; Romans 9:1-15",                                  // 273
    "Isaiah 50-52; Romans 9:16-33",                                 // 274
    "Isaiah 53-55; Romans 10",                                      // 275
    "Isaiah 56-58; Romans 11:1-18",                                 // 276
    "Isaiah 59-61; Romans 11:19-36",                                // 277
    "Isaiah 62-64; Romans 12",                                      // 278
    "Isaiah 65-66; Romans 13",                                      // 279
    "Hosea 1-4; Romans 14",                                         // 280
    "Hosea 5-8; Romans 15:1-13",                                    // 281
    "Hosea 9-11; Romans 15:14-33",                                  // 282
    "Hosea 12-14; Romans 16",                                       // 283
    "Micah 1-3; Acts 21:1-17",                                      // 284
    "Micah 4-5; Acts 21:18-40",                                     // 285
    "Micah 6-7; Acts 22",                                           // 286
    "Nahum 1-3; Acts 23:1-15",                                      // 287
    "2 Chronicles 33-34; Acts 23:16-35",                            // 288
    "Zephaniah 1-3; Acts 24",                                       // 289
    "2 Chronicles 35; Habakkuk 1-3; Acts 25",                       // 290
    "Jeremiah 1-2; Acts 26",                                        // 291
    "Jeremiah 3-5; Acts 27:1-26",                                   // 292
    "Jeremiah 6, 11-12; Acts 27:27-44",                             // 293
    "Jeremiah 7-8, 26; Acts 28",                                    // 294
    "Jeremiah 9-10, 14; Colossians 1",                              // 295
    "Jeremiah 15-17; Colossians 2",                                 // 296
    "Jeremiah 18-19; Colossians 3",                                 // 297
    "Jeremiah 20, 35-36; Colossians 4",                             // 298
    "Jeremiah 25, 45-46; Hebrews 1",                                // 299
    "Jeremiah 47-48; Hebrews 2",                                    // 300
    "Jeremiah 49, 13, 22; Hebrews 3",                               // 301
    "Jeremiah 23-24; Hebrews 4",                                    // 302
    "Jeremiah 27-29; Hebrews 5",                                    // 303
    "Jeremiah 50; Hebrews 6",                                       // 304
    "Jeremiah 51, 30; Hebrews 7",                                   // 305
    "Jeremiah 31-32; Hebrews 8",                                    // 306
    "Jeremiah 33, 21; Hebrews 9",                                   // 307
    "Jeremiah 34, 37-38; Hebrews 10:1-18",                          // 308
    "Jeremiah 39, 52, 40; Hebrews 10:19-39",                        // 309
    "Jeremiah 41-42; Hebrews 11:1-19",                              // 310
    "Jeremiah 43-44; Hebrews 11:20-40",                             // 311
    "Lamentations 1-2; Hebrews 12",                                 // 312
    "Lamentations 3-5; Hebrews 13",                                 // 313
    "2 Chronicles 36; Daniel 1-2; Titus 1",                         // 314
    "Daniel 3-4; Titus 2",                                          // 315
    "Daniel 5-7; Titus 3",                                          // 316
    "Daniel 8-10; Philemon 1",                                      // 317
    "Daniel 11-12; 1 Timothy 1",                                    // 318
    "Psalm 137; Ezekiel 1-2; 1 Timothy 2",                          // 319
    "Ezekiel 3-4; 1 Timothy 3",                                     // 320
    "Ezekiel 5-7; 1 Timothy 4",                                     // 321
    "Ezekiel 8-10; 1 Timothy 5",                                    // 322
    "Ezekiel 11-13; 1 Timothy 6",                                   // 323
    "Ezekiel 14-15; 2 Timothy 1",                                   // 324
    "Ezekiel 16-17; 2 Timothy 2",                                   // 325
    "Ezekiel 18-19; 2 Timothy 3",                                   // 326
    "Ezekiel 20-21; 2 Timothy 4",                                   // 327
    "Ezekiel 22-23; 1 Peter 1",                                     // 328
    "Ezekiel 24-26; 1 Peter 2",                                     // 329
    "Ezekiel 27-29; 1 Peter 3",                                     // 330
    "Ezekiel 30-32; 1 Peter 4",                                     // 331
    "Ezekiel 33-34; 1 Peter 5",                                     // 332
    "Ezekiel 35-36; 2 Peter 1",                                     // 333
    "Ezekiel 37-39; 2 Peter 2",                                     // 334
    "Ezekiel 40-41; 2 Peter 3",                                     // 335
    "Ezekiel 42-44; 1 John 1",                                      // 336
    "Ezekiel 45-46; 1 John 2",                                      // 337
    "Ezekiel 47-48; 1 John 3",                                      // 338
    "Ezra 1-2; 1 John 4",                                           // 339
    "Ezra 3-4; 1 John 5",                                           // 340
    "Haggai 1-2; 2 John 1",                                         // 341
    "Zechariah 1-4; 3 John 1",                                      // 342
    "Zechariah 5-8; Jude 1",                                        // 343
    "Zechariah 9-10; Revelation 1",                                 // 344
    "Zechariah 11-12; Revelation 2",                                // 345
    "Zechariah 13-14; Revelation 3-4",                              // 346
    "Psalm 74-76; Revelation 5",                                    // 347
    "Psalm 77-78; Revelation 6",                                    // 348
    "Psalm 79-80; Revelation 7",                                    // 349
    "Psalm 81-83; Revelation 8",                                    // 350
    "Psalm 84, 90; Revelation 9",                                   // 351
    "Psalm 107, 126; Revelation 10",                                // 352
    "Ezra 5-7; Revelation 11",                                      // 353
    "Esther 1-2; Matthew 1; Luke 3",                                // 354
    "Esther 3-5; Revelation 12",                                    // 355
    "Esther 6-8; Revelation 13",                                    // 356
    "Esther 9-10; Revelation 14",                                   // 357
    "Ezra 8-10; Revelation 15",                                     // 358
    "Nehemiah 1-3; Revelation 16",                                  // 359
    "Nehemiah 4-6; Revelation 17",                                  // 360
    "Nehemiah 7-9; Revelation 18",                                  // 361
    "Nehemiah 10-11; Revelation 19",                                // 362
    "Nehemiah 12-13; Revelation 20",                                // 363
    "Malachi 1-2; Revelation 21",                                   // 364
    "Malachi 3-4; Revelation 22",                                   // 365
};

static const char* const PLAN_SEQUENTIAL[365] = {
    "Genesis 1-3",                                                  // 1
    "Genesis 4-7",                                                  // 2
    "Genesis 8:1-11:9",                                             // 3
    "Genesis 11:10-14:13",                                          // 4
    "Genesis 14:14-18:8",                                           // 5
    "Genesis 18:9-21:21",                                           // 6
    "Genesis 21:22-24:27",                                          // 7
    "Genesis 24:28-26:35",                                          // 8
    "Genesis 27-29",                                                // 9
    "Genesis 30:1-31:42",                                           // 10
    "Genesis 31:43-34:31",                                          // 11
    "Genesis 35:1-37:24",                                           // 12
    "Genesis 37:25-40:8",                                           // 13
    "Genesis 40:9-42:28",                                           // 14
    "Genesis 42:29-45:15",                                          // 15
    "Genesis 45:16-48:7",                                           // 16
    "Genesis 48:8-50:26; Exodus 1",                                 // 17
    "Exodus 2:1-5:9",                                               // 18
    "Exodus 5:10-8:15",                                             // 19
    "Exodus 8:16-11:10",                                            // 20
    "Exodus 12:1-14:20",                                            // 21
    "Exodus 14:21-17:16",                                           // 22
    "Exodus 18:1-21:21",                                            // 23
    "Exodus 21:22-25:9",                                            // 24
    "Exodus 25:10-27:21",                                           // 25
    "Exodus 28-29",                                                 // 26
    "Exodus 30-32",                                                 // 27
    "Exodus 33:1-35:29",                                            // 28
    "Exodus 35:30-37:29",                                           // 29
    "Exodus 38:1-40:16",                                            // 30
    "Exodus 40:17-38; Leviticus 1-4",                               // 31
    "Leviticus 5-7",                                                // 32
    "Leviticus 8:1-11:8",                                           // 33
    "Leviticus 11:9-13:39",                                         // 34
    "Leviticus 13:40-14:57",                                        // 35
    "Leviticus 15:1-18:18",                                         // 36
    "Leviticus 18:19-21:24",                                        // 37
    "Leviticus 22-23",                                              // 38
    "Leviticus 24:1-26:13",                                         // 39
    "Leviticus 26:14-27:34; Numbers 1:1-41",                        // 40
    "Numbers 1:42-3:32",                                            // 41
    "Numbers 3:33-5:22",                                            // 42
    "Numbers 5:23-7:59",                                            // 43
    "Numbers 7:60-10:10",                                           // 44
    "Numbers 10:11-13:16",                                          // 45
    "Numbers 13:17-15:21",                                          // 46
    "Numbers 15:22-16:50",                                          // 47
    "Numbers 17-20",                                                // 48
    "Numbers 21-23",                                                // 49
    "Numbers 24:1-26:34",                                           // 50
    "Numbers 26:35-28:31",                                          // 51
    "Numbers 29:1-31:47",                                           // 52
    "Numbers 31:48-33:56",                                          // 53
    "Numbers 34-36; Deuteronomy 1:1-15",                            // 54
    "Deuteronomy 1:16-3:29",                                        // 55
    "Deuteronomy 4:1-6:15",                                         // 56
    "Deuteronomy 6:16-9:21",                                        // 57
    "Deuteronomy 9:22-12:32",                                       // 58
    "Deuteronomy 13:1-16:8",                                        // 59
    "Deuteronomy 16:9-19:21",                                       // 60
    "Deuteronomy 20:1-23:14",                                       // 61
    "Deuteronomy 23:15-27:10",                                      // 62
    "Deuteronomy 27:11-28:68",                                      // 63
    "Deuteronomy 29:1-32:14",                                       // 64
    "Deuteronomy 32:15-34:12; Joshua 1:1-9",                        // 65
    "Joshua 1:10-4:24",                                             // 66
    "Joshua 5:1-8:23",                                              // 67
    "Joshua 8:24-11:9",                                             // 68
    "Joshua 11:10-14:15",                                           // 69
    "Joshua 15-17",                                                 // 70
    "Joshua 18:1-21:12",                                            // 71
    "Joshua 21:13-23:16",                                           // 72
    "Joshua 24; Judges 1-2",                                        // 73
    "Judges 3-5",                                                   // 74
    "Judges 6-7",                                                   // 75
    "Judges 8-9",                                                   // 76
    "Judges 10-13",                                                 // 77
    "Judges 14-16",                                                 // 78
    "Judges 17:1-20:11",                                            // 79
    "Judges 20:12-21:25; Ruth 1:1-2:13",                            // 80
    "Ruth 2:14-4:22; 1 Samuel 1",                                   // 81
    "1 Samuel 2-4",                                                 // 82
    "1 Samuel 5:1-9:10",                                            // 83
    "1 Samuel 9:11-12:18",                                          // 84
    "1 Samuel 12:19-14:42",                                         // 85
    "1 Samuel 14:43-17:25",                                         // 86
    "1 Samuel 17:26-19:24",                                         // 87
    "1 Samuel 20-22",                                               // 88
    "1 Samuel 23:1-25:31",                                          // 89
    "1 Samuel 25:32-30:10",                                         // 90
    "1 Samuel 30:11-31:13; 2 Samuel 1-2",                           // 91
    "2 Samuel 3:1-6:11",                                            // 92
    "2 Samuel 6:12-10:19",                                          // 93
    "2 Samuel 11-13",                                               // 94
    "2 Samuel 14-16",                                               // 95
    "2 Samuel 17-19",                                               // 96
    "2 Samuel 20:1-22:34",                                          // 97
    "2 Samuel 22:35-24:17",                                         // 98
    "2 Samuel 24:18-25; 1 Kings 1:1-2:18",                          // 99
    "1 Kings 2:19-4:19",                                            // 100
    "1 Kings 4:20-7:39",                                            // 101
    "1 Kings 7:40-9:9",                                             // 102
    "1 Kings 9:10-11:25",                                           // 103
    "1 Kings 11:26-13:34",                                          // 104
    "1 Kings 14-17",                                                // 105
    "1 Kings 18:1-20:25",                                           // 106
    "1 Kings 20:26-22:36",                                          // 107
    "1 Kings 22:37-53; 2 Kings 1:1-4:28",                           // 108
    "2 Kings 4:29-8:15",                                            // 109
    "2 Kings 8:16-10:24",                                           // 110
    "2 Kings 10:25-14:10",                                          // 111
    "2 Kings 14:11-17:18",                                          // 112
    "2 Kings 17:19-19:24",                                          // 113
    "2 Kings 19:25-23:9",                                           // 114
    "2 Kings 23:10-25; 1 Chronicles 1:1-16",                        // 115
    "1 Chronicles 1:17-3:9",                                        // 116
    "1 Chronicles 3:10-6:30",                                       // 117
    "1 Chronicles 6:31-8:28",                                       // 118
    "1 Chronicles 8:29-11:21",                                      // 119
    "1 Chronicles 11:22-15:29",                                     // 120
    "1 Chronicles 16:1-19:9",                                       // 121
    "1 Chronicles 19:10-23:11",                                     // 122
    "1 Chronicles 23:12-26:19",                                     // 123
    "1 Chronicles 26:20-29:19",                                     // 124
    "1 Chronicles 29:20-30; 2 Chronicles 1:1-4:10",                 // 125
    "2 Chronicles 4:11-7:22",                                       // 126
    "2 Chronicles 8:1-11:12",                                       // 127
    "2 Chronicles 11:13-15:19",                                     // 128
    "2 Chronicles 16:1-20:13",                                      // 129
    "2 Chronicles 20:14-24:14",                                     // 130
    "2 Chronicles 24:15-28:27",                                     // 131
    "2 Chronicles 29-31",                                           // 132
    "2 Chronicles 32:1-35:19",                                      // 133
    "2 Chronicles 35:20-36:23; Ezra 1-3",                           // 134
    "Ezra 4-7",                                                     // 135
    "Ezra 8-10; Nehemiah 1",                                        // 136
    "Nehemiah 2-5",                                                 // 137
    "Nehemiah 6:1-8:8",                                             // 138
    "Nehemiah 8:9-11:21",                                           // 139
    "Nehemiah 11:22-13:22",                                         // 140
    "Nehemiah 13:23-31; Esther 1-4",                                // 141
    "Esther 5-10",                                                  // 142
    "Job 1:1-5:16",                                                 // 143
    "Job 5:17-8:22",                                                // 144
    "Job 9:1-12:12",                                                // 145
    "Job 12:13-16:10",                                              // 146
    "Job 16:11-20:11",                                              // 147
    "Job 20:12-24:12",                                              // 148
    "Job 24:13-29:13",                                              // 149
    "Job 29:14-32:10",                                              // 150
    "Job 32:11-35:16",                                              // 151
    "Job 36:1-39:12",                                               // 152
    "Job 39:13-42:9",                                               // 153
    "Job 42:10-17; Psalms 1:1-5:7",                                 // 154
    "Psalms 5:8-8:9",                                               // 155
    "Psalms 9-10",                                                  // 156
    "Psalms 11:1-17:5",                                             // 157
    "Psalms 17:6-18:36",                                            // 158
    "Psalms 18:37-21:13",                                           // 159
    "Psalms 22:1-24:6",                                             // 160
    "Psalms 24:7-27:6",                                             // 161
    "Psalms 27:7-31:5",                                             // 162
    "Psalms 31:6-33:5",                                             // 163
    "Psalms 33:6-35:21",                                            // 164
    "Psalms 35:22-37:26",                                           // 165
    "Psalms 37:27-39:13",                                           // 166
    "Psalms 40-42",                                                 // 167
    "Psalms 43:1-45:12",                                            // 168
    "Psalms 45:13-48:14",                                           // 169
    "Psalms 49:1-51:9",                                             // 170
    "Psalms 51:10-54:7",                                            // 171
    "Psalms 55:1-57:3",                                             // 172
    "Psalms 57:4-60:12",                                            // 173
    "Psalms 61-64",                                                 // 174
    "Psalms 65:1-68:4",                                             // 175
    "Psalms 68:5-69:4",                                             // 176
    "Psalms 69:5-71:16",                                            // 177
    "Psalms 71:17-73:20",                                           // 178
    "Psalms 73:21-76:7",                                            // 179
    "Psalms 76:8-78:24",                                            // 180
    "Psalms 78:25-72",                                              // 181
    "Psalms 79-82",                                                 // 182
    "Psalms 83-86",                                                 // 183
    "Psalms 87:1-89:37",                                            // 184
    "Psalms 89:38-91:13",                                           // 185
    "Psalms 91:14-94:16",                                           // 186
    "Psalms 94:17-98:3",                                            // 187
    "Psalms 98:4-102:7",                                            // 188
    "Psalms 102:8-104:4",                                           // 189
    "Psalms 104:5-105:24",                                          // 190
    "Psalms 105:25-106:33",                                         // 191
    "Psalms 106:34-107:38",                                         // 192
    "Psalms 107:39-109:31",                                         // 193
    "Psalms 110-113",                                               // 194
    "Psalms 114:1-118:9",                                           // 195
    "Psalms 118:10-119:40",                                         // 196
    "Psalms 119:41-96",                                             // 197
    "Psalms 119:97-160",                                            // 198
    "Psalms 119:161-124:8",                                         // 199
    "Psalms 125-131",                                               // 200
    "Psalms 132:1-135:14",                                          // 201
    "Psalms 135:15-138:3",                                          // 202
    "Psalms 138:4-140:13",                                          // 203
    "Psalms 141:1-145:7",                                           // 204
    "Psalms 145:8-148:6",                                           // 205
    "Psalms 148:7-150:6; Proverbs 1:1-2:9",                         // 206
    "Proverbs 2:10-5:14",                                           // 207
    "Proverbs 5:15-8:11",                                           // 208
    "Proverbs 8:12-11:11",                                          // 209
    "Proverbs 11:12-13:25",                                         // 210
    "Proverbs 14-16",                                               // 211
    "Proverbs 17-20",                                               // 212
    "Proverbs 21-23",                                               // 213
    "Proverbs 24:1-27:10",                                          // 214
    "Proverbs 27:11-30:33",                                         // 215
    "Proverbs 31; Ecclesiastes 1:1-3:8",                            // 216
    "Ecclesiastes 3:9-8:17",                                        // 217
    "Ecclesiastes 9-12; Song of Songs 1-2",                         // 218
    "Song of Songs 3-8; Isaiah 1:1-9",                              // 219
    "Isaiah 1:10-5:17",                                             // 220
    "Isaiah 5:18-9:12",                                             // 221
    "Isaiah 9:13-13:16",                                            // 222
    "Isaiah 13:17-19:10",                                           // 223
    "Isaiah 19:11-24:6",                                            // 224
    "Isaiah 24:7-28:22",                                            // 225
    "Isaiah 28:23-32:20",                                           // 226
    "Isaiah 33:1-37:29",                                            // 227
    "Isaiah 37:30-40:31",                                           // 228
    "Isaiah 41-44",                                                 // 229
    "Isaiah 45-49",                                                 // 230
    "Isaiah 50-54",                                                 // 231
    "Isaiah 55:1-60:9",                                             // 232
    "Isaiah 60:10-65:25",                                           // 233
    "Isaiah 66; Jeremiah 1:1-2:25",                                 // 234
    "Jeremiah 2:26-5:19",                                           // 235
    "Jeremiah 5:20-8:22",                                           // 236
    "Jeremiah 9-12",                                                // 237
    "Jeremiah 13:1-16:9",                                           // 238
    "Jeremiah 16:10-20:18",                                         // 239
    "Jeremiah 21-24",                                               // 240
    "Jeremiah 25-27",                                               // 241
    "Jeremiah 28:1-31:20",                                          // 242
    "Jeremiah 31:21-33:26",                                         // 243
    "Jeremiah 34-36",                                               // 244
    "Jeremiah 37:1-41:10",                                          // 245
    "Jeremiah 41:11-45:5",                                          // 246
    "Jeremiah 46-48",                                               // 247
    "Jeremiah 49:1-51:10",                                          // 248
    "Jeremiah 51:11-52:34",                                         // 249
    "Lamentations 1:1-3:51",                                        // 250
    "Lamentations 3:52-5:22; Ezekiel 1-2",                          // 251
    "Ezekiel 3-8",                                                  // 252
    "Ezekiel 9-12",                                                 // 253
    "Ezekiel 13:1-16:43",                                           // 254
    "Ezekiel 16:44-20:17",                                          // 255
    "Ezekiel 20:18-22:12",                                          // 256
    "Ezekiel 22:13-24:27",                                          // 257
    "Ezekiel 25:1-28:10",                                           // 258
    "Ezekiel 28:11-31:18",                                          // 259
    "Ezekiel 32:1-34:24",                                           // 260
    "Ezekiel 34:25-38:9",                                           // 261
    "Ezekiel 38:10-40:49",                                          // 262
    "Ezekiel 41-43",                                                 // 263
    "Ezekiel 44-47",                                                 // 264
    "Ezekiel 48; Daniel 1:1-2:30",                                  // 265
    "Daniel 2:31-4:27",                                             // 266
    "Daniel 4:28-7:12",                                             // 267
    "Daniel 7:13-11:13",                                            // 268
    "Daniel 11:14-12:13; Hosea 1-3",                                // 269
    "Hosea 4-9",                                                    // 270
    "Hosea 10-14; Joel 1:1-2:17",                                   // 271
    "Joel 2:18-3:21; Amos 1-4",                                     // 272
    "Amos 5-9; Obadiah 1:1-9",                                      // 273
    "Obadiah 1:10-21; Jonah 1-4; Micah 1-3",                        // 274
    "Micah 4-7; Nahum 1-2",                                         // 275
    "Nahum 3; Habakkuk 1-3; Zephaniah 1:1-13",                      // 276
    "Zephaniah 1:14-3:20; Haggai 1-2; Zechariah 1:1-11",            // 277
    "Zechariah 1:12-7:14",                                          // 278
    "Zechariah 8-13",                                               // 279
    "Zechariah 14; Malachi 1-4",                                    // 280
    "Matthew 1-4",                                                  // 281
    "Matthew 5-7",                                                  // 282
    "Matthew 8:1-10:15",                                            // 283
    "Matthew 10:16-12:21",                                          // 284
    "Matthew 12:22-14:12",                                          // 285
    "Matthew 14:13-17:13",                                          // 286
    "Matthew 17:14-20:34",                                          // 287
    "Matthew 21-22",                                                // 288
    "Matthew 23:1-25:13",                                           // 289
    "Matthew 25:14-26:68",                                          // 290
    "Matthew 26:69-28:20",                                          // 291
    "Mark 1-3",                                                     // 292
    "Mark 4-5",                                                     // 293
    "Mark 6-7",                                                     // 294
    "Mark 8:1-10:12",                                               // 295
    "Mark 10:13-12:12",                                             // 296
    "Mark 12:13-14:11",                                             // 297
    "Mark 14:12-15:47",                                             // 298
    "Mark 16; Luke 1",                                              // 299
    "Luke 2-3",                                                     // 300
    "Luke 4:1-6:26",                                                // 301
    "Luke 6:27-8:25",                                               // 302
    "Luke 8:26-10:16",                                              // 303
    "Luke 10:17-12:12",                                             // 304
    "Luke 12:13-14:11",                                             // 305
    "Luke 14:12-16:31",                                             // 306
    "Luke 17:1-19:27",                                              // 307
    "Luke 19:28-21:9",                                              // 308
    "Luke 21:10-22:46",                                             // 309
    "Luke 22:47-24:23",                                             // 310
    "Luke 24:24-53; John 1:1-2:11",                                 // 311
    "John 2:12-4:38",                                               // 312
    "John 4:39-6:51",                                               // 313
    "John 6:52-8:20",                                               // 314
    "John 8:21-10:18",                                              // 315
    "John 10:19-12:11",                                             // 316
    "John 12:12-14:11",                                             // 317
    "John 14:12-17:13",                                             // 318
    "John 17:14-19:42",                                             // 319
    "John 20-21; Acts 1:1-2:13",                                    // 320
    "Acts 2:14-4:37",                                               // 321
    "Acts 5:1-7:29",                                                // 322
    "Acts 7:30-9:22",                                               // 323
    "Acts 9:23-11:30",                                              // 324
    "Acts 12-13",                                                   // 325
    "Acts 14:1-16:10",                                              // 326
    "Acts 16:11-18:28",                                             // 327
    "Acts 19-20",                                                   // 328
    "Acts 21:1-23:25",                                              // 329
    "Acts 23:26-27:8",                                              // 330
    "Acts 27:9-28:31; Romans 1:1-15",                               // 331
    "Romans 1:16-5:11",                                             // 332
    "Romans 5:12-8:25",                                             // 333
    "Romans 8:26-11:24",                                            // 334
    "Romans 11:25-15:33",                                           // 335
    "Romans 16; 1 Corinthians 1:1-4:13",                            // 336
    "1 Corinthians 4:14-7:40",                                      // 337
    "1 Corinthians 8:1-12:11",                                      // 338
    "1 Corinthians 12:12-15:28",                                    // 339
    "1 Corinthians 15:29-16:24; 2 Corinthians 1-2",                 // 340
    "2 Corinthians 3-7",                                            // 341
    "2 Corinthians 8-12",                                           // 342
    "2 Corinthians 13; Galatians 1:1-4:11",                         // 343
    "Galatians 4:12-6:18; Ephesians 1:1-14",                        // 344
    "Ephesians 1:15-5:14",                                          // 345
    "Ephesians 5:15-6:24; Philippians 1-2",                         // 346
    "Philippians 3-4; Colossians 1",                                // 347
    "Colossians 2-4; 1 Thessalonians 1:1-2:12",                     // 348
    "1 Thessalonians 2:13-5:28; 2 Thessalonians 1-3",               // 349
    "1 Timothy 1-5",                                                // 350
    "1 Timothy 6; 2 Timothy 1-4",                                   // 351
    "Titus 1-3; Philemon 1; Hebrews 1:1-2:10",                      // 352
    "Hebrews 2:11-6:20",                                            // 353
    "Hebrews 7-10",                                                 // 354
    "Hebrews 11-13",                                                // 355
    "James 1:1-4:10",                                               // 356
    "James 4:11-5:20; 1 Peter 1:1-3:12",                            // 357
    "1 Peter 3:13-5:14; 2 Peter 1-2",                               // 358
    "2 Peter 3; 1 John 1:1-3:12",                                   // 359
    "1 John 3:13-5:21; 2 John 1; 3 John 1; Jude 1:1-16",           // 360
    "Jude 1:17-25; Revelation 1-4",                                 // 361
    "Revelation 5:1-9:11",                                          // 362
    "Revelation 9:12-14:8",                                         // 363
    "Revelation 14:9-18:24",                                        // 364
    "Revelation 19-22",                                             // 365
};

map<string, string> loadConfig() {
    map<string, string> cfg;
    ifstream f(CONFIG_FILE);
    ifstream fHome;
    string homePath;
    if (!f.good()) {
        const char* home = getenv(HOME_ENV);
        if (home) {
            homePath = string(home) + "/" + CONFIG_FILE;
            fHome.open(homePath);
        }
    }
    ifstream& src = f.good() ? f : fHome;
    if (!src.good()) return cfg;
    string line;
    bool inSection = false;
    while (getline(src, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t s = line.find_first_not_of(" \t");
        if (s == string::npos) continue;
        if (line[s] == '[') {
            size_t e = line.find(']', s);
            inSection = (e != string::npos && line.substr(s + 1, e - s - 1) == SECTION);
            continue;
        }
        if (!inSection || line[s] == '#') continue;
        line = line.substr(s);
        size_t eq = line.find('=');
        if (eq == string::npos) continue;
        string key = line.substr(0, eq);
        string val = line.substr(eq + 1);
        size_t ke = key.find_last_not_of(" \t");
        if (ke != string::npos) key = key.substr(0, ke + 1);
        size_t vs = val.find_first_not_of(" \t");
        val = (vs != string::npos) ? val.substr(vs) : "";
        if (!key.empty()) cfg[key] = val;
    }
    return cfg;
}

// Return cfg[key] if present, otherwise defaultVal.
string cfgGet(const map<string, string>& cfg, const string& key, const string& defaultVal) {
    auto it = cfg.find(key);
    return (it != cfg.end()) ? it->second : defaultVal;
}

// Expand a reading plan entry like "1 Samuel 18-20; Psalms 11, 59"
// into individual chapter references: ["1 Samuel 18","1 Samuel 19","1 Samuel 20","Psalms 11","Psalms 59"]
// Whole-book entries (no chapter numbers) and special notations are skipped.
vector<string> expandPlanEntry(const string& entry) {
    vector<string> result;
    stringstream ss(entry);
    string segment;
    while (getline(ss, segment, ';')) {
        size_t s = segment.find_first_not_of(" \t");
        size_t e = segment.find_last_not_of(" \t");
        if (s == string::npos) continue;
        segment = segment.substr(s, e - s + 1);
        if (segment.empty() || segment.find('&') != string::npos) continue;

        // Split segment by comma
        vector<string> parts;
        {
            stringstream cs(segment);
            string cp;
            while (getline(cs, cp, ',')) {
                size_t ts = cp.find_first_not_of(" \t");
                size_t te = cp.find_last_not_of(" \t");
                if (ts != string::npos) parts.push_back(cp.substr(ts, te - ts + 1));
            }
        }
        if (parts.empty()) continue;

        // Determine where to start searching for the chapter digit.
        // Skip a leading "N " prefix so numbered books (1 Samuel, 2 Kings…) are handled.
        size_t searchFrom = 0;
        if (!parts[0].empty() && isdigit((unsigned char)parts[0][0])) {
            size_t sp = parts[0].find(' ');
            if (sp != string::npos && sp + 1 < parts[0].size() &&
                isalpha((unsigned char)parts[0][sp + 1]))
                searchFrom = sp + 1;
            else
                continue; // bare digit or "2, 3 John" style — skip
        }

        // Find where the chapter spec starts in the first part
        size_t chapStart = string::npos;
        for (size_t i = searchFrom; i < parts[0].size(); ++i)
            if (isdigit((unsigned char)parts[0][i])) { chapStart = i; break; }
        if (chapStart == string::npos) continue; // whole-book reference, no chapter numbers

        string bookName = parts[0].substr(0, chapStart);
        if (!bookName.empty() && bookName.back() == ' ') bookName.pop_back();
        if (bookName == "Psalms") bookName = "Psalm";
        string firstSpec = parts[0].substr(chapStart);

        auto expandSpec = [&](const string& spec) {
            for (char c : spec)
                if (!isdigit((unsigned char)c) && c != '-') return;
            size_t dash = spec.find('-');
            try {
                if (dash != string::npos) {
                    int c1 = stoi(spec.substr(0, dash));
                    int c2 = stoi(spec.substr(dash + 1));
                    for (int ch = c1; ch <= c2; ++ch)
                        result.push_back(bookName + " " + to_string(ch));
                } else {
                    result.push_back(bookName + " " + spec);
                }
            } catch (...) {}
        };

        expandSpec(firstSpec);
        for (size_t i = 1; i < parts.size(); ++i) {
            const string& p = parts[i];
            bool onlyNumDash = !p.empty();
            for (char c : p) if (!isdigit((unsigned char)c) && c != '-') { onlyNumDash = false; break; }
            if (onlyNumDash) expandSpec(p);
        }
    }
    return result;
}

// Function to load Bible verses from file
map<string, string> loadBible(const string& filename) {
    map<string, string> verses;
    ifstream file(filename);
    string line;
    while (getline(file, line)) {
        // Remove BOM if present
        if (line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            line = line.substr(3);
        }
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // Parse any line that looks like "Book Chapter:Verse\tText"
        size_t tab = line.find('\t');
        if (tab == string::npos) continue;
        string ref = line.substr(0, tab);
        // Ref must contain a colon to be a valid verse reference
        if (ref.find(':') == string::npos) continue;
        string text = line.substr(tab + 1);
        verses[ref] = text;
    }
    return verses;
}

// Bible verse database
map<string, string> bibleVerses;

// Look up one or more verses for a reference like "Romans 8:9" or "Romans 8:9-10"
// Returns the verse text, or empty string on failure. Writes errors to stderr.
string lookupVerses(const string& reference, bool verseNumbers = false, bool verseNewline = false, bool markdown = false) {
    // Cross-chapter verse range: "Judges 7:21-8:5" → Book Ch1:V1 through Ch2:V2
    {
        size_t firstColon = reference.find(':');
        if (firstColon != string::npos) {
            size_t dash = reference.find('-', firstColon);
            if (dash != string::npos) {
                string afterDash = reference.substr(dash + 1);
                // Cross-chapter if afterDash starts with a digit and contains another colon
                if (!afterDash.empty() && isdigit((unsigned char)afterDash[0]) && afterDash.find(':') != string::npos) {
                    // Parse left side: "Judges 7:21"
                    string leftSide = reference.substr(0, dash);
                    size_t lc = leftSide.rfind(':');
                    string bookName = leftSide.substr(0, lc);
                    // Find last space before chapter number to extract book
                    size_t chStart = bookName.rfind(' ');
                    int ch1 = stoi(leftSide.substr(chStart + 1, lc - chStart - 1));
                    int v1  = stoi(leftSide.substr(lc + 1));
                    // Parse right side: "8:5"
                    size_t rc = afterDash.find(':');
                    int ch2 = stoi(afterDash.substr(0, rc));
                    int v2  = stoi(afterDash.substr(rc + 1));
                    // bookName is everything before the chapter on the left side
                    bookName = leftSide.substr(0, lc);
                    // Strip trailing chapter number to get bare book name
                    size_t sp = bookName.rfind(' ');
                    if (sp != string::npos) bookName = bookName.substr(0, sp);

                    auto getChapterEnd = [&](int ch) {
                        string prefix = bookName + " " + to_string(ch) + ":";
                        int maxV = 0;
                        for (const auto& e : bibleVerses) {
                            if (e.first.compare(0, prefix.size(), prefix) == 0) {
                                int v = stoi(e.first.substr(prefix.size()));
                                if (v > maxV) maxV = v;
                            }
                        }
                        return maxV;
                    };

                    string combined;
                    bool anyMissing = false;
                    for (int ch = ch1; ch <= ch2; ++ch) {
                        int startV = (ch == ch1) ? v1 : 1;
                        int endV   = (ch == ch2) ? v2 : getChapterEnd(ch);
                        if (endV == 0) { anyMissing = true; continue; }
                        string prefix = bookName + " " + to_string(ch) + ":";
                        for (int v = startV; v <= endV; ++v) {
                            string key = prefix + to_string(v);
                            auto it = bibleVerses.find(key);
                            if (it != bibleVerses.end()) {
                                if (!combined.empty()) combined += verseNewline ? (markdown ? "  \n" : "\n") : " ";
                                if (verseNumbers) combined += "[" + to_string(v) + "] ";
                                combined += it->second;
                            } else {
                                cerr << "Warning: verse not found: " << key << endl;
                                anyMissing = true;
                            }
                        }
                    }
                    if (!combined.empty()) {
                        if (anyMissing)
                            cerr << "Warning: some verses in range '" << reference << "' were not found." << endl;
                        return combined;
                    }
                }
            }
        }
    }

    // Check for a verse range (e.g. "Romans 8:9-10" or "Romans 8:20-")
    size_t colon = reference.rfind(':');
    if (colon != string::npos) {
        size_t dash = reference.find('-', colon);
        if (dash != string::npos) {
            string bookChapter = reference.substr(0, colon + 1); // "Romans 8:"
            int startVerse = stoi(reference.substr(colon + 1, dash - colon - 1));

            // Determine end verse: find last verse in chapter if nothing follows the dash
            int endVerse;
            string afterDash = reference.substr(dash + 1);
            if (afterDash.empty()) {
                endVerse = 0;
                for (const auto& entry : bibleVerses) {
                    if (entry.first.compare(0, bookChapter.size(), bookChapter) == 0) {
                        int v = stoi(entry.first.substr(bookChapter.size()));
                        if (v > endVerse) endVerse = v;
                    }
                }
                if (endVerse == 0) {
                    cerr << "Warning: no verses found for chapter in '" << reference << "'." << endl;
                    return "";
                }
            } else {
                endVerse = stoi(afterDash);
            }

            string combined;
            bool anyMissing = false;
            for (int v = startVerse; v <= endVerse; ++v) {
                string key = bookChapter + to_string(v);
                auto it = bibleVerses.find(key);
                if (it != bibleVerses.end()) {
                    if (!combined.empty()) combined += verseNewline ? (markdown ? "  \n" : "\n") : " ";
                    if (verseNumbers) combined += "[" + to_string(v) + "] ";
                    combined += it->second;
                } else {
                    cerr << "Warning: verse not found: " << key << endl;
                    anyMissing = true;
                }
            }
            if (combined.empty()) return "";
            if (anyMissing) {
                cerr << "Warning: some verses in range '" << reference << "' were not found." << endl;
            }
            return combined;
        }
    }
    // No colon: treat as entire chapter (e.g. "Romans 8")
    if (colon == string::npos) {
        string prefix = reference + ":";
        // Collect matching verses, keyed by verse number for correct ordering
        map<int, string> chapterVerses;
        for (const auto& entry : bibleVerses) {
            if (entry.first.compare(0, prefix.size(), prefix) == 0) {
                int verseNum = stoi(entry.first.substr(prefix.size()));
                chapterVerses[verseNum] = entry.second;
            }
        }
        if (chapterVerses.empty()) {
            cerr << "Warning: no verses found for chapter '" << reference << "'." << endl;
            return "";
        }
        string combined;
        for (const auto& v : chapterVerses) {
            if (!combined.empty()) combined += verseNewline ? (markdown ? "  \n" : "\n") : " ";
            if (verseNumbers) combined += "[" + to_string(v.first) + "] ";
            combined += v.second;
        }
        return combined;
    }

    // Single verse lookup — verse number comes from the reference itself
    auto it = bibleVerses.find(reference);
    if (it != bibleVerses.end()) {
        if (verseNumbers) {
            int verseNum = stoi(reference.substr(colon + 1));
            return "[" + to_string(verseNum) + "] " + it->second;
        }
        return it->second;
    }
    return "";
}

string formatCitation(const string& verseText, const string& reference, const string& version, bool markdown, int refStyle, bool italic = false, bool verseQuotes = false) {
    string citation = reference + " (" + version + ")";
    string quotedText = verseQuotes ? "\u201c" + verseText + "\u201d" : verseText;
    string formatted;
    if (refStyle == 2) {
        formatted = quotedText + " - " + citation;
    } else if (refStyle == 3) {
        formatted = quotedText + " (" + reference + ")";
    } else if (refStyle == 4) {
        formatted = quotedText + " (" + citation + ")";
    } else {
        formatted = quotedText + "  \n\u2014 " + citation;
    }
    if (markdown && italic) {
        formatted = "*" + formatted + "*";
    }
    return formatted;
}

string encodeEsvRef(const string& ref) {
    string encoded;
    for (char c : ref) {
        if (c == ' ')      encoded += "%20";
        else if (c == ':') encoded += "%3A";
        else               encoded += c;
    }
    return encoded;
}

string toEsvUrl(const vector<string>& refs) {
    string combined;
    for (size_t i = 0; i < refs.size(); ++i) {
        if (i > 0) combined += ";";
        combined += encodeEsvRef(refs[i]);
    }
    return "https://www.esv.org/verses/" + combined + "/";
}

string toEsvUrl(const string& ref) {
    return toEsvUrl(vector<string>{ref});
}

string toGwUrl(const vector<string>& refs, const string& gwVersion) {
    string combined;
    for (size_t i = 0; i < refs.size(); ++i) {
        if (i > 0) combined += ";";
        combined += encodeEsvRef(refs[i]);
    }
    return "https://www.biblegateway.com/passage/?search=" + combined + "&version=" + gwVersion;
}

string toGwUrl(const string& ref, const string& gwVersion) {
    return toGwUrl(vector<string>{ref}, gwVersion);
}

void openEsvInBrowser(const string& url) {
#ifdef _WIN32
    string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
    string cmd = "open \"" + url + "\"";
#else
    string cmd = "xdg-open \"" + url + "\"";
#endif
    system(cmd.c_str());
}

void printHelp() {
    cout << "bv v" << VERSION << endl;
    cout << "\nUsage: bv REF [OPTIONS]" << endl;
    cout << "       bv --ref=REF [OPTIONS]" << endl;
    cout << "       bv --day[=N]  [OPTIONS]" << endl;
    cout << "\nOptions:" << endl;
    cout << "  -h, --help              Show this help message and exit" << endl;
    cout << "  -v, --version           Show version information and exit" << endl;
    cout << "  -bv=VERSION             Set Bible version (default: KJV)" << endl;
    cout << "  --bibleversion=VERSION  Specify Bible version (KJV, BSB, WEB)" << endl;
    cout << "  REF, --ref=REF          Bible reference (positional or named)" << endl;
    cout << "                          Use comma to separate multiple references" << endl;
    cout << "                          Formats: Book Ch:V  Book Ch:V-V  Book Ch:V-  Book Ch" << endl;
    cout << "  --refstyle=STYLE        Citation style: 1=new line (default), 2=inline, 3=parens, 4=parens+version" << endl;
    cout << "  --versenumbers, -vn     Prefix each verse with its verse number, e.g. [1]" << endl;
    cout << "  --versenewline, -vnl    Start each verse on a new line" << endl;
    cout << "  --italic                Italicize verse text" << endl;
    cout << "  --versequotes           Wrap each verse in curly quotes" << endl;
    cout << "  --chapterheader, -ch    Print book and chapter as a header for full chapters" << endl;
    cout << "  -e, -esv, --openesv     Open reference on esv.org in the browser" << endl;
    cout << "  -g, --opengw            Open reference on biblegateway.com in the browser" << endl;
    cout << "  -d, --day               Show today's reading from the reading plan" << endl;
    cout << "  -d=N, --day=N           Show day N (1-365) from the reading plan" << endl;
    cout << "  --plan=NAME             Select reading plan (default: Chronological)" << endl;
    cout << "                          Sequential (aliases: Canonical, Straight Through)" << endl;
    cout << "                          Old and New Testament (aliases: OTNT, OT and NT)" << endl;
    cout << "  --refonly               With --day: print only the reference string, no verse text" << endl;
    cout << "\nConfig file (.verselumen in current directory, [bv] section):" << endl;
    cout << "  --saveconfig            Save current settings to .verselumen [bv] as new defaults" << endl;
    cout << "  --showconfig            Print current effective settings and exit" << endl;
    cout << "  Supported keys:  bv  refstyle  versequotes  plan" << endl;
    cout << "\nExamples:" << endl;
    cout << "  bv --ref=\"John 3:16\"                    Single verse" << endl;
    cout << "  bv --ref=\"John 3:16,Romans 5:8\"         Multiple verses" << endl;
    cout << "  bv --ref=\"Romans 8:20-\"                  Verse to end of chapter" << endl;
    cout << "  bv --ref=\"Romans 8\" -vn                  Full chapter with verse numbers" << endl;
    cout << "  bv --ref=\"Psalm 7, 27\"                   Shorthand: Psalm 7 and Psalm 27" << endl;
    cout << "  bv -bv=BSB --ref=\"John 3:16\"             Specific Bible version" << endl;
    cout << "  bv --day                                 Today's chronological reading" << endl;
    cout << "  bv --day=42 -vn                          Day 42 with verse numbers" << endl;
}

static bool writeSection(const vector<string>& lines) {
    vector<string> before, after;
    bool inTarget = false, found = false;
    ifstream f(CONFIG_FILE);
    if (f.good()) {
        string line;
        while (getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t s = line.find_first_not_of(" \t");
            if (s != string::npos && line[s] == '[') {
                size_t e = line.find(']', s);
                string sec = (e != string::npos) ? line.substr(s + 1, e - s - 1) : "";
                if (sec == SECTION) { inTarget = true; found = true; continue; }
                else inTarget = false;
            }
            if (inTarget) continue;
            (found ? after : before).push_back(line);
        }
    }
    while (!before.empty() && before.back().empty()) before.pop_back();
    while (!after.empty() && after.front().empty()) after.erase(after.begin());
    ofstream out(CONFIG_FILE);
    if (!out) return false;
    for (const string& l : before) out << l << "\n";
    if (!before.empty()) out << "\n";
    out << "[" << SECTION << "]\n";
    for (const string& l : lines) out << l << "\n";
    if (!after.empty()) { out << "\n"; for (const string& l : after) out << l << "\n"; }
    return true;
}

// Parse --start= value: "today", integer day-of-year, or mm/dd/yyyy.
// Returns (time_t)-1 on parse failure.
static time_t parseStartArg(const string& s) {
    if (s == "today") return time(nullptr);
    if (!s.empty() && s.find_first_not_of("0123456789") == string::npos) {
        int d = stoi(s);
        time_t now = time(nullptr);
        struct tm t = *localtime(&now);
        t.tm_mon = 0; t.tm_mday = d; t.tm_hour = 12; t.tm_min = 0; t.tm_sec = 0;
        return mktime(&t);
    }
    int mm = 0, dd = 0, yyyy = 0;
    if (sscanf(s.c_str(), "%d/%d/%d", &mm, &dd, &yyyy) == 3) {
        if (yyyy < 100) yyyy += 2000;
        struct tm t = {};
        t.tm_year = yyyy - 1900; t.tm_mon = mm - 1; t.tm_mday = dd; t.tm_hour = 12;
        return mktime(&t);
    }
    return (time_t)-1;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    map<string, string> cfg = loadConfig();

    string version  = cfgGet(cfg, "bv",          "KJV");
    int refStyle    = stoi(cfgGet(cfg, "refstyle", "1"));
    bool verseQuotes = cfgGet(cfg, "versequotes", "0") == "1";
    string planArg0 = cfgGet(cfg, "plan", "chronological");
    transform(planArg0.begin(), planArg0.end(), planArg0.begin(), ::tolower);
    string startArg0 = cfgGet(cfg, "start", "");

    string refArg;
    string planArg    = planArg0;
    string startArg   = startArg0;
    int  dayArg       = 0;
    bool dayToday     = false;
    bool verseNumbers  = false;
    bool verseNewline  = false;
    bool italic        = false;
    bool chapterHeader = false;
    bool saveConfig    = false;
    bool showConfig    = false;
    bool refOnly       = false;
    bool openEsv       = false;
    bool openGw        = false;
    bool allDays       = false;
    bool csvOut        = false;
    bool tabOut        = false;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp(); return 0;
        } else if (arg == "-v" || arg == "--version") {
            cout << "bv v" << VERSION << endl; return 0;
        } else if (arg.find("-bv=") == 0) {
            version = arg.substr(arg.find('=') + 1);
        } else if (arg.find("--bibleversion=") == 0) {
            version = arg.substr(arg.find('=') + 1);
        } else if (arg.find("--ref=") == 0) {
            refArg = arg.substr(arg.find('=') + 1);
        } else if (arg.find("--refstyle=") == 0) {
            refStyle = stoi(arg.substr(arg.find('=') + 1));
        } else if (arg == "--versenumbers" || arg == "-vn") {
            verseNumbers = true;
        } else if (arg == "--versenewline" || arg == "-vnl") {
            verseNewline = true;
        } else if (arg == "--italic") {
            italic = true;
        } else if (arg == "--versequotes") {
            verseQuotes = true;
        } else if (arg == "--chapterheader" || arg == "-ch") {
            chapterHeader = true;
        } else if (arg == "--saveconfig") {
            saveConfig = true;
        } else if (arg == "--showconfig") {
            showConfig = true;
        } else if (arg == "--openesv" || arg == "-esv" || arg == "-e") {
            openEsv = true;
        } else if (arg == "--opengw" || arg == "-g") {
            openGw = true;
        } else if (arg.find("--plan=") == 0) {
            planArg = arg.substr(7);
            transform(planArg.begin(), planArg.end(), planArg.begin(), ::tolower);
        } else if (arg == "--refonly") {
            refOnly = true;
        } else if (arg == "--alldays") {
            allDays = true;
        } else if (arg == "--csv") {
            csvOut = true;
        } else if (arg == "--tab") {
            tabOut = true;
        } else if (arg.find("--start=") == 0) {
            startArg = arg.substr(8);
        } else if (arg == "--day" || arg == "-d") {
            dayToday = true;
        } else if (arg.find("--day=") == 0 || arg.find("-d=") == 0) {
            dayArg = stoi(arg.substr(arg.find('=') + 1));
        } else if (refArg.empty() && arg[0] != '-') {
            refArg = arg;
        } else if (arg.find("-") == 0) {
            cerr << "Error: unknown option '" << arg << "'" << endl;
            cerr << "Run 'bv --help' for usage." << endl;
            return 1;
        }
    }

    // Resolve --day / -d (no explicit N) now that --start= is known
    if (dayToday) {
        if (!startArg.empty()) {
            time_t st = parseStartArg(startArg);
            if (st != (time_t)-1) {
                time_t now = time(nullptr);
                dayArg = (int)(difftime(now, st) / 86400.0) + 1;
            }
        }
        if (dayArg == 0) {
            time_t t = time(nullptr);
            dayArg = localtime(&t)->tm_yday + 1;
        }
    }

    if (showConfig) {
        cout << "Effective settings:" << endl;
        cout << "  bv          = " << version              << endl;
        cout << "  refstyle    = " << refStyle             << endl;
        cout << "  versequotes = " << (verseQuotes ? 1 : 0) << endl;
        cout << "  plan        = " << planArg              << endl;
        cout << "  start       = " << (startArg.empty() ? "(not set)" : startArg) << endl;
        ifstream check(CONFIG_FILE);
        cout << "\nConfig file: ./" << CONFIG_FILE
             << (check.good() ? " (loaded)" : " (not found -- using defaults)") << endl;
        return 0;
    }

    if (saveConfig) {
        vector<string> lines = {
            "bv          = " + version,
            "refstyle    = " + to_string(refStyle),
            "versequotes = " + to_string(verseQuotes ? 1 : 0),
            "plan        = " + planArg
        };
        if (!writeSection(lines)) { cerr << "Error: could not write '" << CONFIG_FILE << "'." << endl; return 1; }
        cerr << "Saved [" << SECTION << "] to ./" << CONFIG_FILE << endl;
        return 0;
    }

    if (refArg.empty() && dayArg == 0 && !allDays) {
        cerr << "Usage: bv REF [OPTIONS]" << endl;
        cerr << "       bv --ref=REF [OPTIONS]" << endl;
        cerr << "       bv --day[=N]  [OPTIONS]" << endl;
        cerr << "       bv --alldays [--csv|--tab] [--plan=NAME]" << endl;
        cerr << "Run 'bv --help' for usage." << endl;
        return 1;
    }

    if (allDays) {
        const char* const* plan = PLAN_CHRONOLOGICAL;
        if (planArg == "sequential" || planArg == "canonical" || planArg == "straight through")
            plan = PLAN_SEQUENTIAL;
        else if (planArg == "old and new testament" || planArg == "otnt" || planArg == "ot and nt")
            plan = PLAN_OTNT;

        if (csvOut)      cout << "day,date,reference\n";
        else if (tabOut) cout << "day\tdate\treference\n";

        // Determine base date: start arg if given, else Jan 1 of current year
        time_t now = time(nullptr);
        struct tm yearBase = *localtime(&now);
        struct tm startBase = {};
        bool hasStart = false;
        if (!startArg.empty()) {
            time_t st = parseStartArg(startArg);
            if (st != (time_t)-1) { startBase = *localtime(&st); hasStart = true; }
        }

        for (int d = 1; d <= 365; ++d) {
            struct tm t;
            if (hasStart) {
                t = startBase;
                t.tm_mday += (d - 1);
            } else {
                t = yearBase;
                t.tm_mon  = 0;
                t.tm_mday = d;
                t.tm_hour = 12;
                t.tm_min  = 0;
                t.tm_sec  = 0;
            }
            mktime(&t);
            char dateBuf[32];
            strftime(dateBuf, sizeof(dateBuf), "%m/%d/%Y", &t);
            const char* ref = plan[d - 1];
            if (csvOut)
                cout << d << "," << dateBuf << ",\"" << ref << "\"\n";
            else if (tabOut)
                cout << d << "\t" << dateBuf << "\t" << ref << "\n";
            else
                cout << ref << "\n";
        }
        return 0;
    }

    if (dayArg < 0 || dayArg > 365) {
        cerr << "Error: --day value must be between 1 and 365." << endl;
        return 1;
    }

    transform(version.begin(), version.end(), version.begin(), ::toupper);
    string gwVersion = (version == "BSB") ? "WEB" : version;

    string bibleFile, bibleUrl;
    if (version == "KJV") {
        bibleFile = "BibleKJV.txt"; bibleUrl = "https://raw.githubusercontent.com/codelife-us/VerseLumen/main/BibleKJV.txt";
    } else if (version == "BSB") {
        bibleFile = "BibleBSB.txt"; bibleUrl = "https://bereanbible.com/bsb.txt";
    } else if (version == "WEB") {
        bibleFile = "BibleWEB.txt"; bibleUrl = "https://raw.githubusercontent.com/codelife-us/VerseLumen/main/BibleWEB.txt";
    } else if (openGw) {
        goto bible_ready;  // unknown version passed through to Bible Gateway as-is
    } else {
        cerr << "Error: unsupported Bible version '" << version << "'." << endl;
        cerr << "Supported versions: KJV, BSB, WEB" << endl;
        return 1;
    }

    {
        ifstream test(bibleFile);
        if (!test.good()) {
            // Try $HOME directory
            const char* home = getenv(HOME_ENV);
            if (home) {
                string homePath = string(home) + "/" + bibleFile;
                ifstream homeTest(homePath);
                if (homeTest.good()) {
                    bibleFile = homePath;
                    goto bible_ready;
                }
            }
            cerr << "Bible file '" << bibleFile << "' not found." << endl;
            cerr << "Download it now? (y/n): ";
            char answer; cin >> answer;
            if (answer == 'y' || answer == 'Y') {
                string tmpFile = bibleFile + ".tmp";
                string cmd = "curl -L --fail \"" + bibleUrl + "\" -o \"" + tmpFile + "\"";
                if (system(cmd.c_str()) != 0) {
                    remove(tmpFile.c_str());
                    cerr << "Download failed. Please download manually:\n  " << bibleUrl << endl;
                    return 1;
                }
                // Verify the downloaded file looks like Bible text, not HTML
                {
                    ifstream chk(tmpFile);
                    string first;
                    getline(chk, first);
                    if (first.find('<') != string::npos) {
                        remove(tmpFile.c_str());
                        cerr << "Download returned HTML instead of Bible text.\nPlease download manually:\n  " << bibleUrl << endl;
                        return 1;
                    }
                }
                if (rename(tmpFile.c_str(), bibleFile.c_str()) != 0) {
                    remove(tmpFile.c_str());
                    cerr << "Failed to save " << bibleFile << endl;
                    return 1;
                }
                cout << "Downloaded " << bibleFile << " successfully." << endl;
            } else {
                cerr << "Cannot continue without a Bible file. Exiting." << endl;
                return 1;
            }
        }
    }
    bible_ready:

    bibleVerses = loadBible(bibleFile);

    // Resolve plan name to array
    const char* const* activePlan = PLAN_CHRONOLOGICAL;
    string planName = "Chronological";
    if (planArg == "sequential" || planArg == "canonical" || planArg == "straight through") {
        activePlan = PLAN_SEQUENTIAL;
        planName = "Sequential";
    } else if (planArg == "old and new testament" || planArg == "otnt" || planArg == "ot and nt") {
        activePlan = PLAN_OTNT;
        planName = "Old and New Testament";
    } else if (planArg != "chronological") {
        cerr << "Error: unknown plan '" << planArg << "'." << endl;
        cerr << "Available plans: Chronological (default), Sequential, Old and New Testament" << endl;
        return 1;
    }

    if (dayArg > 0 && refOnly) {
        cout << activePlan[dayArg - 1] << "\n";
        if (refArg.empty()) return 0;
    }

    if (dayArg > 0) {
        string entry = activePlan[dayArg - 1];
        if (entry.empty()) {
            cerr << "Error: no reading plan entry for day " << dayArg << "." << endl;
            return 1;
        }
        cout << "Day " << dayArg << " (" << planName << "): " << entry << "\n";
        vector<string> dayRefs = expandPlanEntry(entry);
        if (openEsv || openGw) {
            // Split raw entry on ';' — works for both chapter and verse-level plans
            vector<string> parts;
            stringstream es(entry);
            string ep;
            while (getline(es, ep, ';')) {
                size_t s = ep.find_first_not_of(" \t");
                size_t e2 = ep.find_last_not_of(" \t");
                if (s != string::npos) parts.push_back(ep.substr(s, e2 - s + 1));
            }
            if (openEsv && !parts.empty()) openEsvInBrowser(toEsvUrl(parts));
            if (openGw  && !parts.empty()) openEsvInBrowser(toGwUrl(parts, gwVersion));
        } else {
            cout << "\n";
            for (const string& ref : dayRefs) {
                string verseText = lookupVerses(ref, verseNumbers, verseNewline, false);
                if (!verseText.empty()) {
                    if (chapterHeader && ref.find(':') == string::npos)
                        cout << ref << "\n\n";
                    cout << formatCitation(verseText, ref, version, false, refStyle, italic, verseQuotes) << "\n\n";
                }
            }
        }
        if (refArg.empty()) return 0;
    }

    stringstream ss(refArg);
    string token, lastBook;
    vector<string> esvRefs, gwRefs;
    while (getline(ss, token, ',')) {
        if (!token.empty() && token.front() == '[') token.erase(0, 1);
        if (!token.empty() && token.back()  == ']') token.pop_back();
        size_t start = token.find_first_not_of(" \t");
        size_t end   = token.find_last_not_of(" \t");
        if (start == string::npos) continue;
        token = token.substr(start, end - start + 1);
        if (!token.empty() && isdigit((unsigned char)token[0]) && !lastBook.empty()) {
            token = lastBook + " " + token;
        } else {
            size_t bookEnd = string::npos;
            for (size_t i = 0; i < token.size(); ++i) {
                if (isdigit((unsigned char)token[i])) {
                    size_t j = i;
                    while (j > 0 && token[j-1] == ' ') --j;
                    if (j > 0) bookEnd = j;
                    break;
                }
            }
            if (bookEnd != string::npos) {
                lastBook = token.substr(0, bookEnd);
                if (lastBook == "Psalms") lastBook = "Psalm";
            }
        }
        if (token.compare(0, 7, "Psalms ") == 0) token = "Psalm " + token.substr(7);

        // Expand chapter ranges like "Judges 19-21" → ["Judges 19","Judges 20","Judges 21"]
        vector<string> expanded;
        if (token.find(':') == string::npos && token.find('-') != string::npos)
            expanded = expandPlanEntry(token);
        if (expanded.empty()) expanded.push_back(token);

        for (const string& t : expanded) {
            if (openEsv) esvRefs.push_back(t);
            if (openGw)  gwRefs.push_back(t);
            if (!openEsv && !openGw) {
                string verseText = lookupVerses(t, verseNumbers, verseNewline, false);
                if (!verseText.empty()) {
                    if (chapterHeader && t.find(':') == string::npos)
                        cout << t << "\n\n";
                    cout << formatCitation(verseText, t, version, false, refStyle, italic, verseQuotes) << endl << endl;
                } else {
                    cerr << "Reference not found: " << t << endl;
                }
            }
        }
    }

    if (openEsv && !esvRefs.empty())
        openEsvInBrowser(toEsvUrl(esvRefs));
    if (openGw && !gwRefs.empty())
        openEsvInBrowser(toGwUrl(gwRefs, gwVersion));

    return 0;
}
