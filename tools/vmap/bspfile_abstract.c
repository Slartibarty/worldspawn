/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* marker */
#define BSPFILE_ABSTRACT_C



/* dependencies */
#include "vmap.h"




/* -------------------------------------------------------------------------------

   this file was copied out of the common directory in order to not break
   compatibility with the q3map 1.x tree. it was moved out in order to support
   the raven bsp format (RBSP) used in soldier of fortune 2 and jedi knight 2.

   since each game has its own set of particular features, the data structures
   below no longer directly correspond to the binary format of a particular game.

   the translation will be done at bsp load/save time to keep any sort of
   special-case code messiness out of the rest of the program.

   ------------------------------------------------------------------------------- */



/* FIXME: remove the functions below that handle memory management of bsp file chunks */

int numBSPDrawVertsBuffer = 0;
void IncDrawVerts(){
	numBSPDrawVerts++;

	if ( bspDrawVerts == 0 ) {
		numBSPDrawVertsBuffer = 1024;

		bspDrawVerts = safe_malloc_info( sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer, "IncDrawVerts" );

	}
	else if ( numBSPDrawVerts > numBSPDrawVertsBuffer ) {
		numBSPDrawVertsBuffer *= 3; // multiply by 1.5
		numBSPDrawVertsBuffer /= 2;

		bspDrawVerts = realloc( bspDrawVerts, sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer );

		if ( !bspDrawVerts ) {
			Error( "realloc() failed (IncDrawVerts)" );
		}
	}

	memset( bspDrawVerts + ( numBSPDrawVerts - 1 ), 0, sizeof( bspDrawVert_t ) );
}

void SetDrawVerts( int n ){
	if ( bspDrawVerts != 0 ) {
		free( bspDrawVerts );
	}

	numBSPDrawVerts = n;
	numBSPDrawVertsBuffer = numBSPDrawVerts;

	bspDrawVerts = safe_malloc_info( sizeof( bspDrawVert_t ) * numBSPDrawVertsBuffer, "IncDrawVerts" );

	memset( bspDrawVerts, 0, n * sizeof( bspDrawVert_t ) );
}

int numBSPDrawSurfacesBuffer = 0;
void SetDrawSurfacesBuffer(){
	if ( bspDrawSurfaces != 0 ) {
		free( bspDrawSurfaces );
	}

	numBSPDrawSurfacesBuffer = MAX_MAP_DRAW_SURFS;

	bspDrawSurfaces = safe_malloc_info( sizeof( bspDrawSurface_t ) * numBSPDrawSurfacesBuffer, "IncDrawSurfaces" );
	bspDrawSurfaceCubemaps = safe_malloc_info( sizeof( *bspDrawSurfaceCubemaps ) * numBSPDrawSurfacesBuffer, "IncDrawSurfaceCubemaps" );

	memset( bspDrawSurfaces, 0, MAX_MAP_DRAW_SURFS * sizeof( bspDrawSurface_t ) );
	memset( bspDrawSurfaceCubemaps, 0xff, MAX_MAP_DRAW_SURFS * sizeof( *bspDrawSurfaceCubemaps ) );
}

void SetDrawSurfaces( int n ){
	if ( bspDrawSurfaces != 0 ) {
		free( bspDrawSurfaces );
	}

	numBSPDrawSurfaces = n;
	numBSPDrawSurfacesBuffer = numBSPDrawSurfaces;

	bspDrawSurfaces = safe_malloc_info( sizeof( bspDrawSurface_t ) * numBSPDrawSurfacesBuffer, "IncDrawSurfaces" );
	bspDrawSurfaceCubemaps = safe_malloc_info( sizeof( *bspDrawSurfaceCubemaps ) * numBSPDrawSurfacesBuffer, "IncDrawSurfaceCubemaps" );

	memset( bspDrawSurfaces, 0, n * sizeof( bspDrawSurface_t ) );
	memset( bspDrawSurfaceCubemaps, 0xff, n * sizeof( *bspDrawSurfaceCubemaps ) );
}

void BSPFilesCleanup(){
	if ( bspDrawVerts != 0 ) {
		free( bspDrawVerts );
	}
	if ( bspDrawSurfaces != 0 ) {
		free( bspDrawSurfaces );
	}
	if ( bspLightBytes != 0 ) {
		free( bspLightBytes );
	}
	if ( bspGridPoints != 0 ) {
		free( bspGridPoints );
	}
}




typedef struct {
	char lumpname[24]; // up to 23 chars, zero-padded
	unsigned int fileofs;  // from file start
	unsigned int filelen;
} bspx_lump_t;
typedef struct bspx_header_s {
	char id[4];  // 'BSPX'
	unsigned int numlumps;
	bspx_lump_t lumps[1];
} bspx_header_t;
void *BSPX_FindLump(const char *lumpname, size_t *lumpsize)
{
	size_t i;
	*lumpsize = 0;
	if (!bspx)
		return NULL;

	for (i = 0; i < bspx->numlumps; i++)
	{
		if (!strncmp(bspx->lumps[i].lumpname, lumpname, 24))
		{
			*lumpsize = bspx->lumps[i].lumpsize;
			return (char*)bspx->lumps[i].data;
		}
	}
	return NULL;
}
void BSPX_ReadSurfExtensions(void)
{
	int *in;
	size_t lumpsize;

	in = BSPX_FindLump("SURFENVMAP", &lumpsize);
	if (in)
	{
		lumpsize /= sizeof(*in);
		if (lumpsize == numBSPDrawSurfaces)
			memcpy(bspDrawSurfaceCubemaps, in, lumpsize * sizeof(*bspDrawSurfaceCubemaps));
	}
}
void BSPX_Setup(void *filebase, size_t filelen, bspLump_t *lumps, size_t stdlumps)
{
	size_t i;
	size_t offs = 0;
	bspx_header_t *h;

	for (i = 0; i < stdlumps; i++, lumps++)
	{
		if (offs < (unsigned int)lumps->offset + (unsigned int)lumps->length)
			offs = (unsigned int)lumps->offset + (unsigned int)lumps->length;
	}
	offs = (offs + 3) & ~3;
	if (offs + sizeof(*h) > filelen)
		return; /*no space for it*/
	h = (bspx_header_t*)((char*)filebase + offs);

	i = LittleLong(h->numlumps);
	/*verify the header*/
	if (*(int*)h->id != (('B'<<0)|('S'<<8)|('P'<<16)|('X'<<24)) ||
	    offs + sizeof(*h) + sizeof(h->lumps[0])*(i-1) > filelen)
		return;
	h->numlumps = i;
	while(i-->0)
	{
		h->lumps[i].fileofs = LittleLong(h->lumps[i].fileofs);
		h->lumps[i].filelen = LittleLong(h->lumps[i].filelen);
		if ((unsigned int)h->lumps[i].fileofs + (unsigned int)h->lumps[i].filelen > filelen)
			return; //truncated or something
	}

	bspx = safe_malloc_info( sizeof(*bspx)-sizeof(*bspx->lumps) + h->numlumps*sizeof(*bspx->lumps), "bspx" );
	bspx->numlumps = h->numlumps;
	for (i = 0; i < bspx->numlumps; i++)
	{
		bspx->lumps[i].lumpsize = h->lumps[i].filelen;
		memcpy(bspx->lumps[i].lumpname, h->lumps[i].lumpname, sizeof(bspx->lumps[i].lumpname));
		bspx->lumps[i].lumpname[sizeof(bspx->lumps[i].lumpname)-1] = 0;
		bspx->lumps[i].data = safe_malloc_info( (bspx->lumps[i].lumpsize+3)&~3u, bspx->lumps[i].lumpname );
		memcpy(bspx->lumps[i].data, (char*)filebase + h->lumps[i].fileofs, bspx->lumps[i].lumpsize);
	}

	BSPX_ReadSurfExtensions();
}
void BSPX_CopyOut(const char *lumpname, void *lumpdata, size_t lumpsize)
{
	size_t i, nl = bspx?bspx->numlumps:0;
	bspx_t *n;
	void *f;

	for (i = 0; i < nl; i++)
	{
		if (!strncmp(bspx->lumps[i].lumpname, lumpname, 24))
			break;
	}

	if (!lumpsize)
	{
		if (bspx && i < nl)
		{       //remove the lump if it exists...
			bspx->numlumps--;
			memcpy(&bspx->lumps[i], &bspx->lumps[i+1], sizeof(*bspx->lumps)*(bspx->numlumps-i));
		}
		return;
	}

	if (i == nl)
	{       //expand the size, if needed
		n = safe_malloc_info( sizeof(*bspx)-sizeof(bspx->lumps) + sizeof(*bspx->lumps)*(nl+1), "bspx" );
		memcpy(n->lumps, bspx->lumps, sizeof(*bspx->lumps)*nl);
		strncpy(n->lumps[nl].lumpname, lumpname, sizeof(n->lumps[nl].lumpname));
		n->lumps[nl].data = NULL;
		n->lumps[nl].lumpsize = 0;
		nl++;
		n->numlumps = nl;
		free(bspx);
		bspx = n;
	}

	f = bspx->lumps[i].data;
	bspx->lumps[i].data = safe_malloc_info( lumpsize+4, bspx->lumps[i].lumpname );
	memcpy(bspx->lumps[i].data, lumpdata, lumpsize);
	memset((char*)bspx->lumps[i].data+lumpsize, 0, 4);      //make sure any padding bytes are 0, to avoid weirdness and boost reproducibility
	bspx->lumps[i].lumpsize = lumpsize;
	free(f);
}
void BSPX_GenerateSurfExtensions(void)
{
	qboolean hasenvmaps = qfalse;
	int i;
	for (i = 0; i < numBSPDrawSurfaces; i++)
	{
		if (bspDrawSurfaceCubemaps[i] != -1)
			hasenvmaps = qtrue;
	}
	BSPX_CopyOut("SURFENVMAP", bspDrawSurfaceCubemaps, hasenvmaps?numBSPDrawSurfaces * sizeof(*bspDrawSurfaceCubemaps):0);
}
void BSPX_WriteLumps(FILE *file, bspLump_t *lumps, size_t stdlumps)
{
	size_t offset = ftell(file);
	unsigned int tmp;
	size_t l;

	BSPX_GenerateSurfExtensions();

	if (!bspx || !bspx->numlumps)
		return; //nothing to write

	//write our bspx header
	SafeWrite(file, "BSPX", 4);
	tmp = LittleLong(bspx->numlumps);
	SafeWrite(file, &tmp, sizeof(tmp));

	offset += 8 + bspx->numlumps*sizeof(bspx_lump_t);
	for (l = 0; l < bspx->numlumps; l++)
	{
		SafeWrite(file, bspx->lumps[l].lumpname, sizeof(bspx->lumps[l].lumpname));
		tmp = LittleLong(offset);
		SafeWrite(file, &tmp, sizeof(tmp));
		offset = (offset+bspx->lumps[l].lumpsize+3)&~3u;        //we're guessing here...
		tmp = LittleLong(bspx->lumps[l].lumpsize);
		SafeWrite(file, &tmp, sizeof(tmp));
	}

	//now write the lumps. hopefully we got the offsets right!...
	for (l = 0; l < bspx->numlumps; l++)
		SafeWrite(file, bspx->lumps[l].data, (bspx->lumps[l].lumpsize+3)&~3u);
}


/*
   SwapBlock()
   if all values are 32 bits, this can be used to swap everything
 */

void SwapBlock( int *block, int size ){
	int i;


	/* dummy check */
	if ( block == NULL ) {
		return;
	}

	/* swap */
	size >>= 2;
	for ( i = 0; i < size; i++ )
		block[ i ] = LittleLong( block[ i ] );
}



/*
   SwapBSPFile()
   byte swaps all data in the abstract bsp
 */

void SwapBSPFile( void ){
	int i, j;


	/* models */
	SwapBlock( (int*) bspModels, numBSPModels * sizeof( bspModels[ 0 ] ) );

	/* shaders (don't swap the name) */
	for ( i = 0; i < numBSPShaders; i++ )
	{
		bspShaders[ i ].contentFlags = LittleLong( bspShaders[ i ].contentFlags );
		bspShaders[ i ].surfaceFlags = LittleLong( bspShaders[ i ].surfaceFlags );
	}

	/* planes */
	SwapBlock( (int*) bspPlanes, numBSPPlanes * sizeof( bspPlanes[ 0 ] ) );

	/* nodes */
	SwapBlock( (int*) bspNodes, numBSPNodes * sizeof( bspNodes[ 0 ] ) );

	/* leafs */
	SwapBlock( (int*) bspLeafs, numBSPLeafs * sizeof( bspLeafs[ 0 ] ) );

	/* leaffaces */
	SwapBlock( (int*) bspLeafSurfaces, numBSPLeafSurfaces * sizeof( bspLeafSurfaces[ 0 ] ) );

	/* leafbrushes */
	SwapBlock( (int*) bspLeafBrushes, numBSPLeafBrushes * sizeof( bspLeafBrushes[ 0 ] ) );

	// brushes
	SwapBlock( (int*) bspBrushes, numBSPBrushes * sizeof( bspBrushes[ 0 ] ) );

	// brushsides
	SwapBlock( (int*) bspBrushSides, numBSPBrushSides * sizeof( bspBrushSides[ 0 ] ) );

	// vis
	( (int*) &bspVisBytes )[ 0 ] = LittleLong( ( (int*) &bspVisBytes )[ 0 ] );
	( (int*) &bspVisBytes )[ 1 ] = LittleLong( ( (int*) &bspVisBytes )[ 1 ] );

	/* drawverts (don't swap colors) */
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[ i ].xyz[ 0 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 0 ] );
		bspDrawVerts[ i ].xyz[ 1 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 1 ] );
		bspDrawVerts[ i ].xyz[ 2 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 2 ] );
		bspDrawVerts[ i ].normal[ 0 ] = LittleFloat( bspDrawVerts[ i ].normal[ 0 ] );
		bspDrawVerts[ i ].normal[ 1 ] = LittleFloat( bspDrawVerts[ i ].normal[ 1 ] );
		bspDrawVerts[ i ].normal[ 2 ] = LittleFloat( bspDrawVerts[ i ].normal[ 2 ] );
		bspDrawVerts[ i ].st[ 0 ] = LittleFloat( bspDrawVerts[ i ].st[ 0 ] );
		bspDrawVerts[ i ].st[ 1 ] = LittleFloat( bspDrawVerts[ i ].st[ 1 ] );
		for ( j = 0; j < MAX_LIGHTMAPS; j++ )
		{
			bspDrawVerts[ i ].lightmap[ j ][ 0 ] = LittleFloat( bspDrawVerts[ i ].lightmap[ j ][ 0 ] );
			bspDrawVerts[ i ].lightmap[ j ][ 1 ] = LittleFloat( bspDrawVerts[ i ].lightmap[ j ][ 1 ] );
		}
	}

	/* drawindexes */
	SwapBlock( (int*) bspDrawIndexes, numBSPDrawIndexes * sizeof( bspDrawIndexes[0] ) );

	/* drawsurfs */
	/* note: rbsp files (and hence q3map2 abstract bsp) have byte lightstyles index arrays, this follows sof2map convention */
	SwapBlock( (int*) bspDrawSurfaces, numBSPDrawSurfaces * sizeof( bspDrawSurfaces[ 0 ] ) );

	/* fogs */
	for ( i = 0; i < numBSPFogs; i++ )
	{
		bspFogs[ i ].brushNum = LittleLong( bspFogs[ i ].brushNum );
		bspFogs[ i ].visibleSide = LittleLong( bspFogs[ i ].visibleSide );
	}
}



/*
   GetLumpElements()
   gets the number of elements in a bsp lump
 */

int GetLumpElements( bspHeader_t *header, int lump, int size ){
	/* check for odd size */
	if ( header->lumps[ lump ].length % size ) {
		if ( force ) {
			Sys_FPrintf( SYS_WRN, "WARNING: GetLumpElements: odd lump size (%d) in lump %d\n", header->lumps[ lump ].length, lump );
			return 0;
		}
		else{
			Error( "GetLumpElements: odd lump size (%d) in lump %d", header->lumps[ lump ].length, lump );
		}
	}

	/* return element count */
	return header->lumps[ lump ].length / size;
}



/*
   GetLump()
   returns a pointer to the specified lump
 */

void *GetLump( bspHeader_t *header, int lump ){
	return (void*)( (byte*) header + header->lumps[ lump ].offset );
}



/*
   CopyLump()
   copies a bsp file lump into a destination buffer
 */

int CopyLump( bspHeader_t *header, int lump, void *dest, int size ){
	int length, offset;


	/* get lump length and offset */
	length = header->lumps[ lump ].length;
	offset = header->lumps[ lump ].offset;

	/* handle erroneous cases */
	if ( length == 0 ) {
		return 0;
	}
	if ( length % size ) {
		if ( force ) {
			Sys_FPrintf( SYS_WRN, "WARNING: CopyLump: odd lump size (%d) in lump %d\n", length, lump );
			return 0;
		}
		else{
			Error( "CopyLump: odd lump size (%d) in lump %d", length, lump );
		}
	}

	/* copy block of memory and return */
	memcpy( dest, (byte*) header + offset, length );
	return length / size;
}

int CopyLump_Allocate( bspHeader_t *header, int lump, void **dest, int size, int *allocationVariable ){
	/* get lump length and offset */
	*allocationVariable = header->lumps[ lump ].length / size;
	*dest = realloc( *dest, size * *allocationVariable );
	return CopyLump( header, lump, *dest, size );
}


/*
   AddLump()
   adds a lump to an outgoing bsp file
 */

void AddLump( FILE *file, bspHeader_t *header, int lumpNum, const void *data, int length ){
	bspLump_t   *lump;


	/* add lump to bsp file header */
	lump = &header->lumps[ lumpNum ];
	lump->offset = LittleLong( ftell( file ) );
	lump->length = LittleLong( length );

	/* write lump to file */
	SafeWrite( file, data, ( length + 3 ) & ~3 );
}



/*
   LoadBSPFile()
   loads a bsp file into memory
 */

void LoadBSPFile( const char *filename ){
	/* dummy check */
	if ( game == NULL || game->load == NULL ) {
		Error( "LoadBSPFile: unsupported BSP file format" );
	}

	/* load it, then byte swap the in-memory version */
	game->load( filename );
	SwapBSPFile();
}



/*
   WriteBSPFile()
   writes a bsp file
 */

void WriteBSPFile( const char *filename ){
	char tempname[ 1024 ];
	time_t tm;


	/* dummy check */
	if ( game == NULL || game->write == NULL ) {
		Error( "WriteBSPFile: unsupported BSP file format" );
	}

	/* make fake temp name so existing bsp file isn't damaged in case write process fails */
	time( &tm );
	sprintf( tempname, "%s.%08X", filename, (int) tm );

	/* byteswap, write the bsp, then swap back so it can be manipulated further */
	SwapBSPFile();
	game->write( tempname );
	SwapBSPFile();

	/* replace existing bsp file */
	remove( filename );
	rename( tempname, filename );
}



/*
   PrintBSPFileSizes()
   dumps info about current file
 */

void PrintBSPFileSizes( void ){
	/* parse entities first */
	if ( numEntities <= 0 ) {
		ParseEntities();
	}

	/* note that this is abstracted */
	Sys_Printf( "Abstracted BSP file components (*actual sizes may differ)\n" );

	/* print various and sundry bits */
	Sys_Printf( "%9d models        %9d\n",
	            numBSPModels, (int) ( numBSPModels * sizeof( bspModel_t ) ) );
	Sys_Printf( "%9d shaders       %9d\n",
	            numBSPShaders, (int) ( numBSPShaders * sizeof( bspShader_t ) ) );
	Sys_Printf( "%9d brushes       %9d\n",
	            numBSPBrushes, (int) ( numBSPBrushes * sizeof( bspBrush_t ) ) );
	Sys_Printf( "%9d brushsides    %9d *\n",
	            numBSPBrushSides, (int) ( numBSPBrushSides * sizeof( bspBrushSide_t ) ) );
	Sys_Printf( "%9d fogs          %9d\n",
	            numBSPFogs, (int) ( numBSPFogs * sizeof( bspFog_t ) ) );
	Sys_Printf( "%9d planes        %9d\n",
	            numBSPPlanes, (int) ( numBSPPlanes * sizeof( bspPlane_t ) ) );
	Sys_Printf( "%9d entdata       %9d\n",
	            numEntities, bspEntDataSize );
	Sys_Printf( "\n" );

	Sys_Printf( "%9d nodes         %9d\n",
	            numBSPNodes, (int) ( numBSPNodes * sizeof( bspNode_t ) ) );
	Sys_Printf( "%9d leafs         %9d\n",
	            numBSPLeafs, (int) ( numBSPLeafs * sizeof( bspLeaf_t ) ) );
	Sys_Printf( "%9d leafsurfaces  %9d\n",
	            numBSPLeafSurfaces, (int) ( numBSPLeafSurfaces * sizeof( *bspLeafSurfaces ) ) );
	Sys_Printf( "%9d leafbrushes   %9d\n",
	            numBSPLeafBrushes, (int) ( numBSPLeafBrushes * sizeof( *bspLeafBrushes ) ) );
	Sys_Printf( "\n" );

	Sys_Printf( "%9d drawsurfaces  %9d *\n",
	            numBSPDrawSurfaces, (int) ( numBSPDrawSurfaces * sizeof( *bspDrawSurfaces ) ) );
	Sys_Printf( "%9d drawverts     %9d *\n",
	            numBSPDrawVerts, (int) ( numBSPDrawVerts * sizeof( *bspDrawVerts ) ) );
	Sys_Printf( "%9d drawindexes   %9d\n",
	            numBSPDrawIndexes, (int) ( numBSPDrawIndexes * sizeof( *bspDrawIndexes ) ) );
	Sys_Printf( "\n" );

	Sys_Printf( "%9d lightmaps     %9d\n",
	            numBSPLightBytes / ( game->lightmapSize * game->lightmapSize * 3 ), numBSPLightBytes );
	Sys_Printf( "%9d lightgrid     %9d *\n",
	            numBSPGridPoints, (int) ( numBSPGridPoints * sizeof( *bspGridPoints ) ) );
	Sys_Printf( "          visibility    %9d\n",
	            numBSPVisBytes );

	if (bspx)
	{
		size_t i;
		Sys_Printf( "\n" );
		for (i = 0; i < bspx->numlumps; i++)
		{       //print counts only for known lumps
			if (!strcmp(bspx->lumps[i].lumpname, "ENVMAP"))
				Sys_Printf( "%9d %-13s %9d\n", bspx->lumps[i].lumpsize/sizeof(denvmap_t), bspx->lumps[i].lumpname, bspx->lumps[i].lumpsize);
			else if (!strcmp(bspx->lumps[i].lumpname, "SURFENVMAP"))
				Sys_Printf( "%9d %-13s %9d\n", bspx->lumps[i].lumpsize/sizeof(int), bspx->lumps[i].lumpname, bspx->lumps[i].lumpsize);
			else
				Sys_Printf( "          %-13s %9d\n", bspx->lumps[i].lumpname, bspx->lumps[i].lumpsize);
		}
	}
}



/* -------------------------------------------------------------------------------

   entity data handling

   ------------------------------------------------------------------------------- */


/*
   StripTrailing()
   strips low byte chars off the end of a string
 */

void StripTrailing( char *e ){
	char    *s;


	s = e + strlen( e ) - 1;
	while ( s >= e && *s <= 32 )
	{
		*s = 0;
		s--;
	}
}



/*
   ParseEpair()
   parses a single quoted "key" "value" pair into an epair struct
 */

epair_t *ParseEPair( void ){
	epair_t     *e;


	/* allocate and clear new epair */
	e = safe_malloc( sizeof( epair_t ) );
	memset( e, 0, sizeof( epair_t ) );

	/* handle key */
	if ( strlen( token ) >= ( MAX_KEY - 1 ) ) {
		Error( "ParseEPair: token too long" );
	}

	e->key = copystring( token );
	GetToken( qfalse );

	/* handle value */
	if ( strlen( token ) >= MAX_VALUE - 1 ) {
		Error( "ParseEpar: token too long" );
	}
	e->value = copystring( token );

	/* strip trailing spaces that sometimes get accidentally added in the editor */
	StripTrailing( e->key );
	StripTrailing( e->value );

	/* return it */
	return e;
}



/*
   ParseEntity()
   parses an entity's epairs
 */

qboolean ParseEntity( void ){
	epair_t     *e;


	/* dummy check */
	if ( !GetToken( qtrue ) ) {
		return qfalse;
	}
	if ( strcmp( token, "{" ) ) {
		Error( "ParseEntity: { not found" );
	}
	AUTOEXPAND_BY_REALLOC( entities, numEntities, allocatedEntities, 32 );

	/* create new entity */
	mapEnt = &entities[ numEntities ];
	numEntities++;
	memset( mapEnt, 0, sizeof( *mapEnt ) );

	/* parse */
	while ( 1 )
	{
		if ( !GetToken( qtrue ) ) {
			Error( "ParseEntity: EOF without closing brace" );
		}
		if ( !EPAIR_STRCMP( token, "}" ) ) {
			break;
		}
		e = ParseEPair();
		e->next = mapEnt->epairs;
		mapEnt->epairs = e;
	}

	/* return to sender */
	return qtrue;
}



/*
   ParseEntities()
   parses the bsp entity data string into entities
 */

void ParseEntities( void ){
	numEntities = 0;
	ParseFromMemory( bspEntData, bspEntDataSize );
	while ( ParseEntity() );

	/* ydnar: set number of bsp entities in case a map is loaded on top */
	numBSPEntities = numEntities;
}



/*
 * must be called before UnparseEntities
 */
void InjectCommandLine( char **argv, int beginArgs, int endArgs ){
	const char *previousCommandLine;
	char newCommandLine[1024];
	const char *inpos;
	char *outpos = newCommandLine;
	char *sentinel = newCommandLine + sizeof( newCommandLine ) - 1;
	int i;

	previousCommandLine = ValueForKey( &entities[0], "_q3map2_cmdline" );
	if ( previousCommandLine && *previousCommandLine ) {
		inpos = previousCommandLine;
		while ( outpos != sentinel && *inpos )
			*outpos++ = *inpos++;
		if ( outpos != sentinel ) {
			*outpos++ = ';';
		}
		if ( outpos != sentinel ) {
			*outpos++ = ' ';
		}
	}

	for ( i = beginArgs; i < endArgs; ++i )
	{
		if ( outpos != sentinel && i != beginArgs ) {
			*outpos++ = ' ';
		}
		inpos = argv[i];
		while ( outpos != sentinel && *inpos )
			if ( *inpos != '\\' && *inpos != '"' && *inpos != ';' && (unsigned char) *inpos >= ' ' ) {
				*outpos++ = *inpos++;
			}
	}

	*outpos = 0;
}



/*
   UnparseEntities()
   generates the dentdata string from all the entities.
   this allows the utilities to add or remove key/value
   pairs to the data created by the map editor
 */

void UnparseEntities( void ){
	int i;
	char        *buf, *end;
	epair_t     *ep;
	char line[ 2048 ];
	char key[ 1024 ], value[ 1024 ];
	const char  *value2;


	/* setup */
	AUTOEXPAND_BY_REALLOC( bspEntData, 0, allocatedBSPEntData, 1024 );
	buf = bspEntData;
	end = buf;
	*end = 0;


	/* run through entity list */
	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		{
			int sz = end - buf;
			AUTOEXPAND_BY_REALLOC( bspEntData, sz + 65536, allocatedBSPEntData, 1024 );
			buf = bspEntData;
			end = buf + sz;
		}

		/* get epair */
		ep = entities[ i ].epairs;
		if ( ep == NULL ) {
			continue;   /* ent got removed */

		}
		/* ydnar: certain entities get stripped from bsp file */
		value2 = ValueForKey( &entities[ i ], "classname" );
		if ( !Q_stricmp( value2, "misc_model" ) ||
		     !Q_stricmp( value2, "prop_static" ) ||
		     !Q_stricmp( value2, "_decal" ) ||
		     !Q_stricmp( value2, "_skybox" ) ) {
			continue;
		}

		/* add beginning brace */
		strcat( end, "{\n" );
		end += 2;

		/* walk epair list */
		for ( ep = entities[ i ].epairs; ep != NULL; ep = ep->next )
		{
			/* copy and clean */
			strcpy( key, ep->key );
			StripTrailing( key );
			strcpy( value, ep->value );
			StripTrailing( value );

			/* add to buffer */
			sprintf( line, "\"%s\" \"%s\"\n", key, value );
			strcat( end, line );
			end += strlen( line );
		}

		/* add trailing brace */
		strcat( end,"}\n" );
		end += 2;

		/* check for overflow */
		if ( end > buf + allocatedBSPEntData ) {
			Error( "Entity text too long" );
		}
	}

	/* set size */
	bspEntDataSize = end - buf + 1;
}



/*
   PrintEntity()
   prints an entity's epairs to the console
 */

void PrintEntity( const entity_t *ent ){
	epair_t *ep;


	Sys_Printf( "------- entity %p -------\n", ent );
	for ( ep = ent->epairs; ep != NULL; ep = ep->next )
		Sys_Printf( "%s = %s\n", ep->key, ep->value );

}



/*
   SetKeyValue()
   sets an epair in an entity
 */

void SetKeyValue( entity_t *ent, const char *key, const char *value ){
	epair_t *ep;


	/* check for existing epair */
	for ( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if ( !EPAIR_STRCMP( ep->key, key ) ) {
			free( ep->value );
			ep->value = copystring( value );
			return;
		}
	}

	/* create new epair */
	ep = safe_malloc( sizeof( *ep ) );
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring( key );
	ep->value = copystring( value );
}



/*
   KeyExists()
   returns true if entity has this key
 */

qboolean KeyExists( const entity_t *ent, const char *key ){
	epair_t *ep;

	/* walk epair list */
	for ( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if ( !EPAIR_STRCMP( ep->key, key ) ) {
			return qtrue;
		}
	}

	/* no match */
	return qfalse;
}



/*
   ValueForKey()
   gets the value for an entity key
 */

const char *ValueForKey( const entity_t *ent, const char *key ){
	epair_t *ep;


	/* dummy check */
	if ( ent == NULL ) {
		return "";
	}

	/* walk epair list */
	for ( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if ( !EPAIR_STRCMP( ep->key, key ) ) {
			return ep->value;
		}
	}

	/* if no match, return empty string */
	return "";
}



/*
   IntForKey()
   gets the integer point value for an entity key
 */

int IntForKey( const entity_t *ent, const char *key ){
	const char  *k;


	k = ValueForKey( ent, key );
	return atoi( k );
}



/*
   FloatForKey()
   gets the floating point value for an entity key
 */

vec_t FloatForKey( const entity_t *ent, const char *key ){
	const char  *k;


	k = ValueForKey( ent, key );
	return atof( k );
}



/*
   GetVectorForKey()
   gets a 3-element vector value for an entity key
 */

qboolean GetVectorForKey( const entity_t *ent, const char *key, vec3_t vec ){
	const char  *k;
	double v1, v2, v3;


	/* get value */
	k = ValueForKey( ent, key );

	/* scanf into doubles, then assign, so it is vec_t size independent */
	v1 = v2 = v3 = 0.0;
	sscanf( k, "%lf %lf %lf", &v1, &v2, &v3 );
	vec[ 0 ] = v1;
	vec[ 1 ] = v2;
	vec[ 2 ] = v3;

	/* true if the key is found, false otherwise */
	return strlen( k );
}



/*
   FindTargetEntity()
   finds an entity target
 */

entity_t *FindTargetEntity( const char *target ){
	int i;
	const char  *n;


	/* walk entity list */
	for ( i = 0; i < numEntities; i++ )
	{
		n = ValueForKey( &entities[ i ], "targetname" );
		if ( !strcmp( n, target ) ) {
			return &entities[ i ];
		}
	}

	/* nada */
	return NULL;
}



/*
   GetEntityShadowFlags() - ydnar
   gets an entity's shadow flags
   note: does not set them to defaults if the keys are not found!
 */

void GetEntityShadowFlags( const entity_t *ent, const entity_t *ent2, int *castShadows, int *recvShadows ){
	const char  *value;

	/* get cast shadows */
	if ( castShadows != NULL ) {
		value = ValueForKey( ent, "zhlt_lightflags" );
		if ( value[ 0 ] != '\0' ) {
			/* bit 2 indicates opaqueness, convert it to a boolean */
			*castShadows = !!( atoi( value ) & 0x2 );
		}
		else {
			value = ValueForKey( ent, "_castShadows" );
			if ( value[ 0 ] == '\0' ) {
				value = ValueForKey( ent, "_cs" );
			}
			if ( value[ 0 ] == '\0' ) {
				value = ValueForKey( ent2, "_castShadows" );
			}
			if ( value[ 0 ] == '\0' ) {
				value = ValueForKey( ent2, "_cs" );
			}
			if ( value[ 0 ] != '\0' ) {
				*castShadows = atoi( value );
			}
		}
	}

	/* receive */
	if ( recvShadows != NULL ) {
		value = ValueForKey( ent, "_receiveShadows" );
		if ( value[ 0 ] == '\0' ) {
			value = ValueForKey( ent, "_rs" );
		}
		if ( value[ 0 ] == '\0' ) {
			value = ValueForKey( ent2, "_receiveShadows" );
		}
		if ( value[ 0 ] == '\0' ) {
			value = ValueForKey( ent2, "_rs" );
		}
		if ( value[ 0 ] != '\0' ) {
			*recvShadows = atoi( value );
		}
	}

	/* vortex: game-specific default eneity keys */
	value = ValueForKey( ent, "classname" );
	if ( !Q_stricmp( game->magic, "dq" ) || !Q_stricmp( game->magic, "prophecy" ) ) {
		/* vortex: deluxe quake default shadow flags */
		if ( !Q_stricmp( value, "func_wall" ) ) {
			if ( recvShadows != NULL ) {
				*recvShadows = 1;
			}
			if ( castShadows != NULL ) {
				*castShadows = 1;
			}
		}
	}
}
