<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:fo="http://www.w3.org/1999/XSL/Format"
                version="1.0">

<!-- Our CSS -->
<xsl:param name="html.stylesheet" select="'dcpomatic.css'"/>

<!-- I can't fathom xmlto's logic with image scaling, so I've turned it off -->
<xsl:param name="ignore.image.scaling" select="1"/>

<!-- <note> in a div with no heading -->
<xsl:template name="note.frobozz">
  <xsl:param name="content">
    <xsl:apply-templates/>
  </xsl:param>
  <xsl:text disable-output-escaping="yes">&lt;div class="note"&gt;</xsl:text>
  <xsl:copy-of select="$content"/>
  <xsl:text disable-output-escaping="yes">&lt;/div&gt;</xsl:text>
</xsl:template>

<xsl:template match="note">
  <xsl:call-template name="note.frobozz"/>
</xsl:template>

</xsl:stylesheet>
