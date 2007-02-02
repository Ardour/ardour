" Tim Mayberry's .vimrc mappings for use with DocBook 4.3. This has been 
" revised from Vivek Venugopalan's .vimrc which was revised from Dan York's .vimrc 
" Revised: August 23, 2006
" Used with vim 7.0
" email : mojofunk@gmail.com

" MAPPINGS
" Like the .vimrc file shown at http://www.vim.org/ I decided to
" start all my mappings with a comma. Since I do pretty much all
" my work in DocBook, I just started with the letter after the 
" comma for a DB tag, rather than using something like 'd' to 
" indicate it was a DB tag (i.e. ',dp' instead of ',p'). If you 
" want to use other mappings, you may want to change this.
" My mappings are currently primarily for easy of entering DB
" tags. I haven't yet gotten into changing existing text with mappings.

" A side effect of using the comma for mappings is that when you type
" a comma in vim, it will now pause and wait for input. If you just hit
" the spacebar, you should see a regular old comma appear.

" Note: 'imap' = a mapping for 'insert' mode of vim
" All of these commands work ONLY when you are in Insert mode

" <CR> will put a line return in the file. This is purely my style of
" entering certain DocBook tags.  You may wish to remove some.

" After typing the DocBook tag, many of these macros then switch to 
" vim command mode, reposition the cursor to where I want it to be, 
" and then re-inter insert mode. You may wish to change where it ends.

let mapleader = ","

" header and setup info for a book
imap<leader>dtbk <!DOCTYPE BOOK PUBLIC "-//OASIS//DTD DocBook V4.2//EN">
imap<leader>bk <book><CR><bookinfo><CR><title></title><CR><author><CR><firstname></firstname><CR><surname></surname><CR></author><CR><address><email></email></address><CR><copyright><CR><year></year><CR><holder></holder><CR></copyright><CR><revhistory><CR></revhistory><CR></bookinfo><CR><CR></book><esc>12k$bba
"Internal subset declaration
imap<leader>et <!ENTITY TODO-key "TODO-value"><CR>
imap<leader>rev <revision><CR><revnumber></revnumber><CR><date></date><CR><authorinitials></authorinitials><CR><revremark></revremark><CR></revision><esc>4k$bba

"header and setup info for an article.
imap<leader>dtart <!DOCTYPE ARTICLE PUBLIC "-//OASIS//DTD DocBook V4.1//EN">
imap<leader>art <article><CR><title></title><CR><CR><artheader><CR><CR><author><CR><firstname></firstname><CR><surname></surname><CR><affiliation><CR><address><email></email></address></affiliation><CR></author><CR><CR><revhistory><CR></revhistory><CR><CR></artheader><CR><abstract><CR><indexterm><CR><primary></primary><CR></indexterm><CR><para><CR><para><CR></abstract><CR><CR></article><esc>16k$bba

"Paragraph formatting
imap<leader>p <para><CR></para><esc>k$a

" character formatting
imap<leader>em <emphasis></emphasis><esc>bba
imap<leader>es <emphasis role="strong"></emphasis><esc>bbla

"Special characters
imap<leader>> &gt;
imap<leader>< &lt;

" links
imap<leader>ul <ulink url=""></ulink><esc>bb3la
imap<leader>lk <link linkend=""></link><esc>bb3la
imap<leader>x <xref linkend=""/><esc>bla

" lists
" note that '<leader>l2' was created solely to fit into<leader>il and<leader>ol
imap<leader>li <listitem><CR><para><CR></para><CR></listitem><esc>kk$a
imap<leader>l2 <listitem><CR><para><CR></para><CR></listitem>
imap<leader>il <itemizedlist><CR><leader>l2<CR></itemizedlist><esc>kkk$a
imap<leader>ol <orderedlist><CR><leader>l2<CR></orderedlist><esc>kkk$a
imap<leader>ve <varlistentry><CR><term></term><CR><leader>l2<CR></varlistentry>
imap<leader>vl <variablelist><CR><title></title><CR><leader>ve<CR></variablelist>

" sections
imap<leader>sn <section id=""><CR><title></title><CR><para><CR></para><CR></section><esc>kkkk$bla
"imap<leader>s1 <sect1 id=""><CR><title></title><CR><para><CR></para><CR></sect1><esc>kkkk$bla
"imap<leader>s2 <sect2 id=""><CR><title></title><CR><para><CR></para><CR></sect2><esc>kkkk$bla
"imap<leader>s3 <sect3 id=""><CR><title></title><CR><para><CR></para><CR></sect3><esc>kkkk$bla
imap<leader>ch <chapter id=""><CR><title></title><CR><para><CR></para><CR></chapter><esc>kkkk$bla

" images
" My mediaobject has two imagedata entries - 1 for EPS and 1 for JPG
imap<leader>img <mediaobject><CR><imageobject><CR><imagedata fileref=""/><CR></imageobject><CR></mediaobject><esc>kk$bla
"imap<leader>img <imageobject><CR><imagedata fileref="" format=""><CR></imageobject>
imap<leader>mo <mediaobject><CR><leader>img<esc>k$hiEPS<esc>j$a<CR>,img<esc>k$hiJPG<esc>j$a<CR></mediaobject>

" other objects
imap<leader>ti <title></title><esc>bba
imap<leader>fo <footnote><CR><para><CR></para><CR></footnote><esc>kk$a
imap<leader>sb <sidebar><CR><title></title><CR><para></para><CR></sidebar>
imap<leader>co <!--  --><esc>bhi
imap<leader>qt <blockquote><CR><attribution></attribution><CR><literallayout><CR></literallayout><CR></blockquote>
imap<leader>ge <glossentry id=""><CR><glossterm></glossterm><CR><glossdef><CR><para><CR></para><CR></glossdef><CR></glossentry><esc>kkkkkk$bba
imap<leader>gt <glossterm linkend=""></glossterm><esc>bb3la
imap<leader>l <literal></literal><esc>bba

" admonitions
imap<leader>no <note><CR><para></para><CR></note><esc>k$bba
imap<leader>tp <tip><CR><para></para><CR></tip><esc>k$bba
imap<leader>imp <important><CR><para></para><CR></important><esc>k$bba
imap<leader>ca <caution><CR><para></para><CR></caution><esc>k$bba
"imap<leader>w <warning><CR><para></para><CR></warning><esc>k$bba

" computer stuff
imap<leader>app <application></application><esc>bba
imap<leader>cm <command></command><esc>bba
imap<leader>sc <screen><CR></screen><esc>k$a
imap<leader>fn <filename></filename><esc>bba
imap<leader>gb <guibutton></guibutton><esc>bba
imap<leader>gl <guilabel></guilabel><esc>bba
imap<leader>gm <guimenuitem></guimenuitem><esc>bba
imap<leader>mb <mousebutton></mousebutton><esc>bba
imap<leader>mc <menuchoice><guimenu></guimenu><guisubmenu></guisubmenu></menuchoice><esc>8ba
imap<leader>kc <keycombo><keycap></keycap><keycap></keycap></keycombo><esc>8ba
imap<leader>kk <keycap></keycap><esc>bba

imap<leader>row <row><CR><entry><CR></entry><CR></row><esc>kk$a
imap<leader>en <entry><CR></entry><esc>k$a

" examples
imap<leader>ex <example id=""><CR><title></title><CR></example><ESC>$kkba

"For preparing FAQs
imap<leader>faq <article class=faq><CR><title>Frequently asked questions</title><CR><CR><articleinfo><CR><CR><author><CR><firstname></firstname><CR><surname></surname><CR><affiliation><CR><address><email></email></address></affiliation><CR></author><CR><CR><revhistory><CR></revhistory><CR><CR></articleinfo><CR><abstract><CR><indexterm><CR><primary></primary><CR></indexterm><CR><para><CR><para><CR></abstract><CR><CR><qandaset><CR><qandadiv><CR><title></title><CR><qandaentry><CR><question><CR><para></para><CR></question><CR><answer><CR><para></para><CR></answer><CR></qandaentry><CR><qandadiv><CR><qandaset><CR><CR></article><esc>16k$bba

imap<leader>qd <qandaset><CR><qandadiv><CR><title></title><CR><qandaentry><CR><question><CR><para></para><CR></question><CR><answer><CR><para></para><CR></answer><CR></qandaentry><CR><qandadiv><esc>9k$bba

imap<leader>qa <qandaentry><CR><question><CR><para></para><CR></question><CR><answer><CR><para></para><CR></answer><CR></qandaentry><esc>5k$bba
