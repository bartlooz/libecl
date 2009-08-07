#include <assert.h>
#include <string.h>
#include <util.h>
#include <parser.h>

#define PARSER_ESCAPE_CHAR '\\'



struct parser_struct
{
  char * splitters;         /* The string is split into tokens on the occurence of one of these characters - and they are removed. */
  char * specials;          /* This exactly like the splitters - but these characters are retained as tokens. */
  char * delete_set;        /* The chracters are just plain removed - but without any splitting on them. */
  char * quoters;       
  char * comment_start; 
  char * comment_end;   
};



parser_type * parser_alloc(
  const char * splitters,        /** Set to NULL if not interessting.            */
  const char * quoters,          /** Set to NULL if not interessting.            */
  const char * specials,         /** Set to NULL if not interessting.            */
  const char * delete_set,
  const char * comment_start,    /** Set to NULL if not interessting.            */
  const char * comment_end)      /** Set to NULL  if not interessting.           */
{
  parser_type * parser = util_malloc(sizeof * parser, __func__);


  if( splitters != NULL)
  {
    if( strlen(splitters) == 0)
      util_abort("%s: Need at least one non '\\0' splitters character.\n", __func__);
    parser->splitters = util_alloc_string_copy( splitters ); 
  }
  else
    parser->splitters = NULL;
  
  if (delete_set != NULL)
    parser->delete_set = util_alloc_string_copy( delete_set );
  else
    parser->delete_set = NULL;


  if( quoters != NULL )
  {
    if( strlen( quoters ) == 0)
      util_abort("%s: Need at least one non '\\0' quote character.\n", __func__);
    parser->quoters = util_alloc_string_copy( quoters ); 
  }
  else
    parser->quoters = NULL;

  if( specials != NULL )
  {
    if( strlen( specials ) == 0)
      util_abort("%s: Need at least one non '\\0' special character.\n", __func__);
    parser->specials = util_alloc_string_copy( specials ); 
  }
  else
    parser->specials = NULL;

  if( comment_start != NULL )
  {
    if( strlen( comment_start ) == 0)
      util_abort("%s: Need at least one non '\\0' character to start a comment.\n", __func__);
    parser->comment_start = util_alloc_string_copy( comment_start );
  }
  else
    parser->comment_start = NULL;
    
  if( comment_end != NULL )
  {
    if( strlen( comment_end ) == 0)
      util_abort("%s: Need at least one non '\\0' character to end a comment.\n", __func__);
    parser->comment_end   = util_alloc_string_copy( comment_end );
  }
  else 
    parser->comment_end   = NULL;

  if(comment_start == NULL && comment_end != NULL)
    util_abort("%s: Need to have comment_start when comment_end is set.\n", __func__);
  if(comment_start != NULL && comment_end == NULL)
    util_abort("%s: Need to have comment_end when comment_start is set.\n", __func__);
  

  return parser;
}



void parser_free(
  parser_type * parser)
{

  util_safe_free( parser->splitters    );
  util_safe_free( parser->quoters       ); 
  util_safe_free( parser->specials      ); 
  util_safe_free( parser->comment_start );
  util_safe_free( parser->comment_end   );
  util_safe_free( parser->delete_set    );

  free( parser     );
}



static
bool is_escape(
  const char c)
{
  if( c == PARSER_ESCAPE_CHAR )
    return true;
  else
    return false;
}




static
int length_of_initial_splitters(
  const char           * buffer_position,
  const parser_type * parser)
{
  assert( buffer_position != NULL );
  assert( parser       != NULL );

  if( parser->splitters == NULL)
    return 0;
  else
    return strspn( buffer_position, parser->splitters );
}

static bool in_set(char c , const char * set) {
  if (set == NULL)
    return false;
  else {
    if (strchr( set , (int) c) != NULL)
      return true;
    else
      return false;
  }
}


static
bool is_splitters(
  const char             c,
  const parser_type * parser)
{
  return in_set(c , parser->splitters);
}

static 
bool is_special(
  const char             c,
  const parser_type * parser)
{
  return in_set(c , parser->specials);
}


static
bool is_in_quoters(
  const char       c,
  const parser_type * parser)
{
  return in_set(c , parser->quoters);
}



static bool is_in_delete_set(const char c , const parser_type * parser) {
  return in_set(c , parser->delete_set);
}


/**
  This finds the number of characters up til
  and including the next occurence of buffer[0].

  E.g. using this funciton on

  char * example = "1231abcd";

  should return 4.

  If the character can not be found, the function will fail with
  util_abort() - all quotation should be terminated (Joakim - WITH
  moustache ...). Observe that this function does not allow for mixed
  quotations, i.e. both ' and " might be vald as quaotation
  characters; but one quoted string must be wholly quoted with EITHER
  ' or ".

  Escaped occurences of the first character are
  not counted. E.g. if PARSER_ESCAPE_CHAR
  occurs in front of a new occurence of the first
  character, this is *NOT* regarded as the end.
*/

static
int length_of_quotation(
  const char * buffer)
{
  assert( buffer != NULL );

  int  length  = 1;
  char target  = buffer[0];
  char current = buffer[1]; 

  bool escaped = false;
  while(current != '\0' &&  !(current == target && !escaped ))
  {
    escaped = is_escape(current);
    length += 1;
    current = buffer[length];
  }
  length += 1;

  if ( current == '\0') /* We ran through the whole string without finding the end of the quotation - abort HARD. */
    util_abort("%s: could not find quotation closing on %s \n",__func__ , buffer);
  
  
  return length;
}



static
int length_of_comment(
  const char           * buffer_position,
  const parser_type * parser)
{  
  bool in_comment = false;
  int length = 0;

  if(parser->comment_start == NULL || parser->comment_end == NULL)
    length = 0;
  else
  {
    const char * comment_start     = parser->comment_start;
    int          len_comment_start = strlen( comment_start );
    if( strncmp( buffer_position, comment_start, len_comment_start) == 0)
    {
      in_comment = true;
      length     = len_comment_start;
    }
    else
      length = 0;
  }

  if( in_comment )
  {
    const char * comment_end       = parser->comment_end;
    int          len_comment_end   = strlen( comment_end   );
    while(buffer_position[length] != '\0' && in_comment)
    {
      if( strncmp( &buffer_position[length], comment_end, len_comment_end) == 0)
      {
        in_comment = false;
        length += len_comment_end; 
      }
      else
        length += 1;
    }
  }
  return length;
}



static
char * alloc_quoted_token(
  const char * buffer,
  int          length,
  bool         strip_quote_marks)
{
  char * token;
  if(!strip_quote_marks)
  {
    token = util_malloc( (length + 1) * sizeof * token, __func__ );
    memmove(token, &buffer[0], length * sizeof * token );
    token[length] = '\0';
  }
  else
  {
    token = util_malloc( (length - 1) * sizeof * token, __func__ );
    memmove(token, &buffer[1], (length -1) * sizeof * token);
    token[length-2] = '\0';
    /**
      Removed escape char before any escaped quotation starts.
    */
    {
      char expr[3];
      char subs[2];
      expr[0] = PARSER_ESCAPE_CHAR;
      expr[1] = buffer[0];
      expr[2] = '\0';
      subs[0] = buffer[0];
      subs[1] = '\0';
      util_string_replace_inplace(&token, expr, subs);
    }
  }
  return token;
}




/** 
    This does not care about the possible occurence of characters in
    the delete_set. That is handled when the token is inserted in the
    token list.
*/
    
static
int length_of_normal_non_splitters(
  const char           * buffer,
  const parser_type * parser)
{
  bool at_end  = false;
  int length   = 0;
  char current = buffer[0];

  while(current != '\0' && !at_end)
  {
    length += 1;
    current = buffer[length];

    if( is_splitters( current, parser ) )
    {
      at_end = true;
      continue;
    }
    if( is_special( current, parser ) )
    {
      at_end = true;
      continue;
    }
    if( is_in_quoters( current, parser ) )
    {
      at_end = true;
      continue;
    }
    if( length_of_comment(&buffer[length], parser) > 0)
    {
      at_end = true;
      continue;
    }
  }

  return length;
}



static int length_of_delete( const char * buffer , const parser_type * parser) {
  int length   = 0;
  char current = buffer[0];

  while(is_in_delete_set( current , parser ) && current != '\0') {
    length += 1;
    current = buffer[length];
  }
  return length;
}


/**
   Allocates a new stringlist. 
*/
stringlist_type * parser_tokenize_buffer(
  const parser_type * parser,
  const char           * buffer,
  bool                   strip_quote_marks)
{
  int position          = 0;
  int buffer_size       = strlen(buffer);
  int splitters_length  = 0;
  int comment_length    = 0;
  int delete_length     = 0;

  stringlist_type * tokens = stringlist_alloc_new();
  
  while( position < buffer_size )
  {
    /** 
      Skip initial splitters.
    */
    splitters_length = length_of_initial_splitters( &buffer[position], parser );
    if(splitters_length > 0)
    {
      position += splitters_length;
      continue;
    }


    /**
      Skip comments.
    */
    comment_length = length_of_comment( &buffer[position], parser);
    if(comment_length > 0)
    {
      position += comment_length;
      continue;
    }

    
    /**
       Skip characters which are just deleted. 
    */
      
    delete_length = length_of_delete( &buffer[position] , parser );
    if (delete_length > 0) {
      position += delete_length;
      continue;
    }



    /** 
       Copy the character if it is in the special set,
    */
    if( is_special( buffer[position], parser ) )
    {
      char key[2];
      key[0] = buffer[position];
      key[1] = '\0';
      stringlist_append_copy( tokens, key );
      position += 1;
      continue;
    }

    /**
       If the character is a quotation start, we copy the whole quotation.
    */
    if( is_in_quoters( buffer[position], parser ) )
    {
      int length   = length_of_quotation( &buffer[position] );
      char * token = alloc_quoted_token( &buffer[position], length, strip_quote_marks );
      stringlist_append_owned_ref( tokens, token );
      position += length;
      continue;
    }

    /**
      If we are here, we are guaranteed that that
      buffer[position] is not:

      1. Whitespace.
      2. The start of a comment.
      3. A special character.
      4. The start of a quotation.
      5. Something to delete.

      In other words, it is the start of plain
      non-splitters. Now we need to find the
      length of the non-splitters until:

      1. Whitespace starts.
      2. A comment starts.
      3. A special character occur.
      4. A quotation starts.
    */

    {
      int length   = length_of_normal_non_splitters( &buffer[position], parser );
      char * token = util_malloc( (length + 1) * sizeof * token, __func__ );
      int token_length;
      if (parser->delete_set == NULL) {
        token_length = length;
        memcpy( token , &buffer[position] , length * sizeof * token );
      } else {
        int i;
        token_length = 0;
        for (i = 0; i < length; i++) {
          char c = buffer[position + i];
          if ( !is_in_delete_set( c , parser)) {
            token[token_length] = c;
            token_length++;
          }
        }
      }


      if (token_length > 0) { /* We do not insert empty tokens. */
        token[token_length] = '\0';
        stringlist_append_owned_ref( tokens, token );
      } else 
        free( token );    /* The whole thing is discarded. */

      position += length;
      continue;
    }
  }

  return tokens;
}



stringlist_type * parser_tokenize_file(
  const parser_type * parser,
  const char           * filename,
  bool                   strip_quote_marks)
{
  stringlist_type * tokens;
  char * buffer = util_fread_alloc_file_content( filename, NULL );
  tokens = parser_tokenize_buffer( parser, buffer, strip_quote_marks );
  free(buffer);
  return tokens;
}


/*****************************************************************/
/* Below are some functions which do not actually tokenize, but use
   the comment/quote handling of the parser implementation for
   related tasks.
*/

static bool fseek_quote_end( char quoter , FILE * stream ) {
  int c;
  do {
    c = fgetc( stream );
  } while (c != quoter && c != EOF);

  if (c == quoter)
    return true;
  else
    return false;
}

static bool fgetc_while_equal( FILE * stream , const char * string ) {
  bool     equal        = true;
  long int current_pos  = ftell(stream);
  
  for (int string_index = 0; string_index < strlen(string); string_index++) {
    int c = fgetc( stream );
    if (c != string[string_index]) {
      equal = false;
      break;
    }
  }
  
  if (!equal) /* OK - not equal - go back. */
    fseek( stream , current_pos , SEEK_SET);
  return equal;
}





/**
   This function is quite tolerant - it will accept (with a warning)
   unterminated comments and unterminated quotations.
*/

bool parser_fseek_string(const parser_type * parser , FILE * stream , const char * string , bool skip_string) {
  long int initial_pos     = ftell( stream );   /* Store the inital position. */
  bool string_found        = false;
  bool cont                = true;
  
  if (strstr( string , parser->comment_start ) != NULL)
    util_abort("%s: sorry the string contains a comment start - will never find it ... \n"); /* A bit harsh ?? */
  
  do {
    int c = fgetc( stream );

    /* Special treatment of quoters: */
    if (is_in_quoters( c , parser )) {
      long int quote_start_pos = ftell(stream);
      if (!fseek_quote_end( c , stream )) {
        fseek( stream ,  quote_start_pos , SEEK_SET);
        fprintf(stderr,"Warning: unterminated quotation starting at line: %d \n",util_get_current_linenr( stream ));
        fseek(stream , 0 , SEEK_END);
      }
      /* Now we are either at the first character following a
         terminated quotation, or at EOF. */
      continue;
    }

    /* Special treatment of comments: */
    if (c == parser->comment_start[0]) {
      /* OK - this might be the start of a comment - let us check further. */
      bool comment_start = fgetc_while_equal( stream , &parser->comment_start[1]);
      if (comment_start) {
        long int comment_start_pos = ftell(stream) - strlen( parser->comment_start );
        /* Start seeking for comment_end */
        if (!util_fseek_string(stream , parser->comment_end , skip_string)) { 
          /* 
             No end comment end was found - what to do about that??
             The file is just positioned at the end - and the routine
             will exit at the next step - with a Warning. 
          */
          fseek( stream , comment_start_pos , SEEK_SET);
          fprintf(stderr,"Warning: unterminated comment starting at line: %d \n",util_get_current_linenr( stream ));
          fseek(stream , 0 , SEEK_END);
        } continue;
        /* Now we are at the character following a comment end - or at EOF. */
      } 
    }
    
    /*****************************************************************/
    
    /* Now c is a regular character - and we can start looking for our string. */
    if (c == string[0]) {  /* OK - we got the first character right - lets try in more detail: */
      bool equal = fgetc_while_equal( stream , &string[1]);
      if (equal) {
        string_found = true;
        cont = false;
      } 
    }
    if (c == EOF) 
      cont = false;
    
  } while (cont);
  
  if (string_found) {
    if (!skip_string)
      fseek(stream , -strlen(string) , SEEK_CUR); /* Reposition to the beginning of 'string' */
  } else
    fseek(stream , initial_pos , SEEK_SET);       /* Could not find the string reposition at initial position. */
  
  return string_found;
}



/**
   This function takes an input buffer, and updates the buffer (in place) according to:

   1. Quoted content is copied verbatim (this takes presedence).
   2. All comment sections are removed.
   3. Delete characters are deleted.
   
*/


void parser_strip_buffer(const parser_type * parser , char ** __buffer) {
  char * src     = *__buffer;
  char * target  = util_malloc( sizeof * target * ( strlen( *__buffer ) + 1) , __func__);

  int src_position    = 0;
  int target_position = 0;
  while (src_position < strlen( src )) {
    int comment_length;
    int delete_length;

    /**
      Skip comments.
    */
    comment_length = length_of_comment( &src[src_position], parser);
    if(comment_length > 0)
    {
      src_position += comment_length;
      continue;
    }

    
    /**
       Skip characters which are just deleted. 
    */
    delete_length = length_of_delete( &src[src_position] , parser );
    if (delete_length > 0) {
      src_position += delete_length;
      continue;
    }
    
    /*
      Quotations.
    */
    if( is_in_quoters( src[src_position], parser ) )
    {
      int length   = length_of_quotation( &src[src_position] );
      char * token = alloc_quoted_token( &src[src_position], length, false );
      memcpy( &target[target_position] , &src[src_position] , length);
      free( token );
      src_position    += length;
      target_position += length;
      continue;
    }

    /**
       OK -it is a god damn normal charactar - copy it straight over: 
    */
    target[target_position] = src[src_position];
    src_position    += 1;
    target_position += 1;
  }
  target[target_position] = '\0';
  target = util_realloc( target , sizeof * target * (target_position + 1) , __func__);
  
  free( src );
  *__buffer = target;
}



/*****************************************************************/
/**
   This file reads file content into a buffer, and then strips the
   buffer with parser_strip_buffer() and returns the 'cleaned up'
   buffer.

   This function is a replacement for the old
   util_fread_alloc_file_content() which no longer has support for a
   comment string.
*/

char * parser_fread_alloc_file_content(const char * filename , const char * quote_set , const char * delete_set , const char * comment_start , const char * comment_end) {
  parser_type * parser = parser_alloc( NULL , quote_set , NULL , delete_set , comment_start , comment_end);
  char * buffer              = util_fread_alloc_file_content( filename , NULL);
  
  parser_strip_buffer( parser , &buffer );
  parser_free( parser );
  return buffer;
}
