<xsl:stylesheet version='1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
     xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'
     xmlns:lut='http://www.opendds.com/modeling/schemas/Lut/1.0'
     xmlns:opendds='http://www.opendds.org/modeling/schemas/OpenDDS/1.0'
     xmlns:generator='http://www.opendds.org/modeling/schemas/Generator/1.0'>
  <!--
    ** $Id$
    **
    ** Generate C++ implementation code.
    **
    ** @TODO - check string management.
    ** @TODO - determine how to set the transport addresses.
    **
    -->
<xsl:include href="common.xsl"/>

<xsl:output method="text"/>
<xsl:strip-space elements="*"/>

<!-- Extract the name of the model once. -->
<xsl:variable name="model" select="document(/generator:CodeGen/source/@name)/opendds:OpenDDSModel"/>
<xsl:variable name="modelname" select="$model/@name"/>
<xsl:variable name="normalized-modelname">
  <xsl:call-template name="normalize-identifier">
    <xsl:with-param name="identifier" select="$model/@name"/>
  </xsl:call-template>
</xsl:variable>
<xsl:variable name="MODELNAME" select="translate($modelname, $lower, $upper)"/>
<xsl:variable name="instances" select="//instance"/>

<!-- process the entire genfile document to produce the C++ header. -->
<xsl:template match="/">
  <xsl:value-of select="concat('#ifndef ', $MODELNAME, '_TRAITS_H', $newline)"/>
  <xsl:value-of select="concat('#define ', $MODELNAME, '_TRAITS_H', $newline)"/>
  <xsl:value-of select="concat('#include &quot;', $modelname, '_T.h&quot;', $newline)"/>

  <xsl:apply-templates select="$model"/>
<xsl:text>
#endif
</xsl:text>
</xsl:template>

<!-- For packages containing DCPS lib, output a namespace -->
<xsl:template match="packages[.//libs[@xsi:type='opendds:DcpsLib']]">
  <xsl:value-of select="concat('namespace ', @name, ' {', $newline)"/>
  <xsl:apply-templates/>
  <xsl:value-of select="concat('}', $newline)"/>
</xsl:template>

<!-- For DCPS lib, output a namespace -->
<xsl:template match="libs[@xsi:type='opendds:DcpsLib']">
  <xsl:value-of select="concat('namespace ', @name, ' {', $newline)"/>
  <xsl:variable name="elements-qname">
    <xsl:call-template name="elements-qname"/>
  </xsl:variable>

  <xsl:for-each select="$instances">
    <xsl:call-template name="output-instance">
      <xsl:with-param name="instance" select="."/>
      <xsl:with-param name="elements-qname" select="$elements-qname"/>
    </xsl:call-template>
  </xsl:for-each>
  <xsl:value-of select="concat('}', $newline)"/>
</xsl:template>

<!-- For a given instance, output class declaration -->
<xsl:template name="output-instance">
  <xsl:param name="instance"/>
  <xsl:param name="elements-qname"/>

  <xsl:variable name="Instname">
    <xsl:call-template name="capitalize">
      <xsl:with-param name="value" select="$instance/@name"/>
    </xsl:call-template>
  </xsl:variable>
  <xsl:variable name="instance-offset" 
                select="$instance/transportOffset/@value"/>
  <xsl:choose>
    <xsl:when test="not($instance-offset)">
      <xsl:value-of select="concat(
                '   // Skipping generation for instance &quot;',
                $Instname,
                '&quot; with no offset',
                $newline)"/>
    </xsl:when>
    <xsl:otherwise>
      <xsl:variable name="structname" select="concat($Instname, 
                                                     $modelname, 
                                                     'Traits')"/>
      <xsl:variable name="export" select="concat($modelname, '_Export ')"/>
      <xsl:variable name="tdname" select="concat($Instname, $modelname, 'Type')"/>

      <!-- output struct declaration -->
      <xsl:value-of select="concat($newline,
        '  struct ', $export, $structname, 
        ' : OpenDDS::Model::DefaultInstanceTraits {', $newline,
        '    enum { transport_key_base = ', 
        $instance-offset, '};', $newline,
        '    void transport_config(OpenDDS::DCPS::TransportIdType id);', $newline,
        '  };', $newline
      )"/>

      <!-- output typedef-->
      <xsl:value-of select="concat($newline,
        '  typedef OpenDDS::Model::Service&lt; ', $elements-qname, ', ',
        $structname, '&gt; ', $tdname, ';', $newline
      )"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<!-- Ignore text -->
<xsl:template match="text()"/>

</xsl:stylesheet>
