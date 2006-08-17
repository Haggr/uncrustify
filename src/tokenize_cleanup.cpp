/**
 * @file tokenize_cleanup.cpp
 * Looks at simple sequences to refine the chunk types.
 * Examples:
 *  - change '[' + ']' into '[]'/
 *  - detect "version = 10;" vs "version (xxx) {"
 *
 * $Id$
 */

#include "uncrustify_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include <cstring>

static void check_template(chunk_t *start);


void tokenize_cleanup(void)
{
   chunk_t *pc   = chunk_get_head();
   chunk_t *prev = NULL;
   chunk_t *next;
   chunk_t *tmp;

   pc   = chunk_get_head();
   next = chunk_get_next_ncnl(pc);
   while ((pc != NULL) && (next != NULL))
   {
      /* Change '[' + ']' into '[]' */
      if ((pc->type == CT_SQUARE_OPEN) && (next->type == CT_SQUARE_CLOSE))
      {
         pc->type = CT_TSQUARE;
         pc->str  = "[]";
         pc->len  = 2;
         chunk_del(next);
         next = chunk_get_next_ncnl(pc);
      }

      if ((pc->type == CT_DOT) && ((cpd.lang_flags & LANG_ALLC) != 0))
      {
         pc->type = CT_MEMBER;
      }

      /* Determine the version stuff (D only) */
      if (pc->type == CT_VERSION)
      {
         if (next->type == CT_PAREN_OPEN)
         {
            pc->type = CT_IF;
         }
         else
         {
            if (next->type != CT_ASSIGN)
            {
               LOG_FMT(LERR, "%s: [%d] version: Unexpected token %s\n",
                       __func__, pc->orig_line, get_token_name(next->type));
            }
            pc->type = CT_WORD;
         }
      }

      /**
       * Change CT_WORD after CT_ENUM, CT_UNION, or CT_STRUCT to CT_TYPE
       * Change CT_WORD before CT_WORD to CT_TYPE
       */
      if (next->type == CT_WORD)
      {
         if ((pc->type == CT_ENUM) ||
             (pc->type == CT_UNION) ||
             (pc->type == CT_STRUCT))
         {
            next->type = CT_TYPE;
         }
         if (pc->type == CT_WORD)
         {
            pc->type = CT_TYPE;
         }
      }

      /**
       * Change CT_STAR to CT_PTR_TYPE if preceeded by CT_TYPE,
       * CT_QUALIFIER, or CT_PTR_TYPE.
       */
      if ((next->type == CT_STAR) &&
          ((pc->type == CT_TYPE) ||
           (pc->type == CT_QUALIFIER) ||
           (pc->type == CT_PTR_TYPE)))
      {
         next->type = CT_PTR_TYPE;
      }

      /**
       * Change angle open followed by
       */
      if (pc->type == CT_ANGLE_OPEN)
      {
         check_template(pc);
      }
      if ((pc->type == CT_ANGLE_CLOSE) && (pc->parent_type != CT_TEMPLATE))
      {
         pc->type = CT_COMPARE;
      }

      if ((cpd.lang_flags & LANG_D) != 0)
      {
         /* Check for the D string concat symbol '~' */
         if ((pc->type == CT_INV) &&
             ((prev->type == CT_STRING) ||
              (next->type == CT_STRING)))
         {
            pc->type = CT_CONCAT;
         }

         /* Check for the D template symbol '!' */
         if ((pc->type == CT_NOT) &&
             (prev->type == CT_WORD) &&
             (next->type == CT_PAREN_OPEN))
         {
            pc->type = CT_D_TEMPLATE;
         }
      }

      /* Change get/set to CT_WORD if not followed by a brace open */
      if ((pc->type == CT_GETSET) && (next->type != CT_BRACE_OPEN))
      {
         pc->type = CT_WORD;
      }

      /* Change item after operator (>=, ==, etc) to a FUNC_OPERATOR */
      if (pc->type == CT_OPERATOR)
      {
         next->type        = CT_FUNCTION;
         next->parent_type = CT_OPERATOR;

         if (chunk_is_addr(prev))
         {
            prev->type = CT_BYREF;
         }
      }

      /* Change private, public, protected into either a qualifier or label */
      if (pc->type == CT_PRIVATE)
      {
         if (next->type == CT_COLON)
         {
            pc->type   = CT_LABEL;
            next->type = CT_LABEL_COLON;
            if ((tmp = chunk_get_next_ncnl(next)) != NULL)
            {
               tmp->flags |= PCF_STMT_START | PCF_EXPR_START;
            }
         }
         else
         {
            pc->type = CT_QUALIFIER;
         }
      }


      /* TODO: determine other stuff here */

      prev = pc;
      pc   = next;
      next = chunk_get_next_ncnl(pc);
   }
}


/**
 * If there is nothing but CT_WORD and CT_MEMBER, then it's probably a
 * template thingy.  Otherwise, it's likely a comparison.
 */
static void check_template(chunk_t *start)
{
   chunk_t *pc;
   chunk_t *end;
   chunk_t *prev;

   prev = chunk_get_prev_ncnl(start);

   if ((prev != NULL) && (prev->type == CT_TEMPLATE))
   {
      end = chunk_get_next_type(start, CT_ANGLE_CLOSE, -1);
   }
   else
   {
      for (pc = chunk_get_next_ncnl(start); pc != NULL; pc = chunk_get_next_ncnl(pc))
      {
         if ((pc->type != CT_WORD) &&
             (pc->type != CT_MEMBER) &&
             (pc->type != CT_COMMA) &&
             (pc->type != CT_STAR) &&
             (pc->type != CT_DC_MEMBER))
         {
            break;
         }
      }
      end = pc;
   }

   if (end != NULL)
   {
      if (end->type == CT_ANGLE_CLOSE)
      {
         for (pc = start; pc != end; pc = chunk_get_next_ncnl(pc))
         {
            pc->parent_type = CT_TEMPLATE;
         }
         end->parent_type = CT_TEMPLATE;
      }
      else
      {
         start->type = CT_COMPARE;
      }
   }
}
