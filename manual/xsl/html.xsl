
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
				xmlns:exsl="http://exslt.org/common"
				version="1.0"
				exclude-result-prefixes="exsl">

<xsl:import href="/usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml/docbook.xsl"/>
<xsl:import href="/usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml/chunk-common.xsl"/>
<xsl:import href="/usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml/chunk-code.xsl"/>
<xsl:import href="/usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml/manifest.xsl"/>

<xsl:param name="html.stylesheet" select="'ardour_manual.css'"/>
<xsl:param name="html.stylesheet.type" select="'text/css'"/>
<xsl:param name="html.cleanup" select="1"/>
<xsl:param name="html.ext" select="'.html'"/>
<xsl:output method="html" indent="yes"/>

<!-- Admonition Graphics -->
<xsl:param name="admon.graphics" select="1"/>
<xsl:param name="admon.graphics.path">./images/tango-icons/</xsl:param>
<xsl:param name="callout.graphics.path">./images/tango-icons/</xsl:param>

<!-- Remove table and inline style from admonitions -->

<xsl:template name="graphical.admonition">
	<xsl:variable name="admon.type">
		<xsl:choose>
			<xsl:when test="local-name(.)='note'">Note</xsl:when>
			<xsl:when test="local-name(.)='warning'">Warning</xsl:when>
			<xsl:when test="local-name(.)='caution'">Caution</xsl:when>
			<xsl:when test="local-name(.)='tip'">Tip</xsl:when>
			<xsl:when test="local-name(.)='important'">Important</xsl:when>
			<xsl:otherwise>Note</xsl:otherwise>
		</xsl:choose>
	</xsl:variable>
	<div xmlns="http://www.w3.org/1999/xhtml" class="{name(.)}">
		<xsl:call-template name="anchor"/>
			<xsl:if test="$admon.textlabel != 0 or title">
				<h2 class="title">
					<xsl:apply-templates select="." mode="object.title.markup"/>
				</h2>
			</xsl:if>
		<xsl:apply-templates/>
	</div>
</xsl:template>

<!-- 
	I'm not using draft mode because with at least the version
	of the stylesheets I have it inserts inline css. I'm not aware
	of a non-hacky way around that so until I find a better 
	solution I'm using custom status fields:

	ardour-draft

	ardour-beta?
	ardour-rc (release candidate)?

-->

<!-- Add css class for status --> 
<xsl:template name="body.attributes"> 
	<xsl:if test="(ancestor-or-self::*[@status][1]/@status != '')"> 
		<xsl:attribute name="class">
			<xsl:value-of select="ancestor-or-self::*[@status][1]/@status"/>
		</xsl:attribute>
	</xsl:if>
</xsl:template>

<!-- titles after all elements -->
<xsl:param name="formal.title.placement">
figure after
example after
equation after
table after
procedure before 
</xsl:param>

<!-- This sets the filename based on the ID. -->
<xsl:param name="use.id.as.filename" select="'1'"/>

<xsl:template match="command">
	<xsl:call-template name="inline.monoseq"/>
</xsl:template>

<xsl:template match="application">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guibutton">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guiicon">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guilabel">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guimenu">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guimenuitem">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="guisubmenu">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="mousebutton">
	<xsl:call-template name="inline.boldseq"/>
</xsl:template>

<xsl:template match="filename">
	<xsl:call-template name="inline.monoseq"/>
</xsl:template>

<!-- TOC -->
<xsl:param name="section.autolabel" select="1"/>
<xsl:param name="section.label.includes.component.label" select="1"/>
<xsl:param name="generate.legalnotice.link" select="1"/>
<xsl:param name="generate.revhistory.link" select="1"/>
<xsl:param name="generate.toc">
set toc
book toc
article toc
chapter toc
qandadiv toc
qandaset toc
sect1 nop
sect2 nop
sect3 nop
sect4 nop
sect5 nop
section toc
part toc
</xsl:param>

<!-- Limit TOC depth to 1 level -->
<xsl:param name="toc.section.depth">1</xsl:param>

<!-- 
<xsl:template name="nongraphical.admonition">
	<div class="{name(.)}">
		<h2 class="title">
			<xsl:call-template name="anchor"/>
			<xsl:if test="$admon.textlabel != 0 or title">
				<xsl:apply-templates select="." mode="object.title.markup"/>
			</xsl:if>
		</h2>
		<xsl:apply-templates/>
	</div>
</xsl:template>
-->
</xsl:stylesheet>
