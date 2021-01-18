#ifndef MUMD_H

#ifndef MUMD_MAX_NESTED_LISTS
#define MUMD_MAX_NESTED_LISTS 8
#endif

#ifndef MUMD_MAX_NESTED_INLINE_STYLE
#define MUMD_MAX_NESTED_INLINE_STYLE 8
#endif

typedef enum {
	md_text,
	md_code_start, md_code_end,
	md_bold_start, md_bold_end,
	md_italics_start, md_italics_end,

	md_link_start, md_link_end,
	md_image_start, md_image_end,

	md_head_1_start, md_head_1_end,
	md_head_2_start, md_head_2_end,
	md_head_3_start, md_head_3_end,
	md_head_4_start, md_head_4_end,
	md_head_5_start, md_head_5_end,
	md_head_6_start, md_head_6_end,

	md_paragraph_start, md_paragraph_end,
	md_code_block_start, md_code_block_end,
	md_quote_start, md_quote_end,

	md_unordered_list_start, md_unordered_list_end, 
	md_ordered_list_start, md_ordered_list_end, 
	md_list_item_start, md_list_item_end, 

	md_horizontal_rule,
} md_type;

typedef struct {
	md_type type;
	// todo: union with other kinds of params
	char *text;
	int length;
} md_node;


void mumd_parse(const char *md, int length, void (*cb)(const md_node node));

#endif MUMD_H

#ifdef MUMD_IMPLEMENTATION

typedef struct {
	md_type type;
	int spaces;
} md_internal_indent;

typedef struct {
	const char *md;
	int length;
	void (*cb)(const md_node node);
	int i;
	int indent_level;
	md_type style_stack[MUMD_MAX_NESTED_INLINE_STYLE];
	md_internal_indent indent_stack[MUMD_MAX_NESTED_LISTS];
} md_internal_state;

static inline int mumd_internal_is_newline(char c) {
	return c == '\n' || c == '\r';
}

static inline int mumd_internal_is_number(char c) {
	return c >= '0' && c <= '9';
}

static inline int mumd_internal_is_whitespace(char c) {
	return c == ' ' || c == '\t';
}

static inline int mumd_internal_parse_link(md_internal_state *state,
	int *linkStart, int *linkLength, int *titleStart, int *titleLength) {

	if (state->i>=state->length || state->md[state->i] != '[') return 0;

	*titleStart=state->i+1;
	*titleLength=0;


	while(*titleStart + *titleLength < state->length && state->md[*titleStart + *titleLength] != ']' &&
		!mumd_internal_is_newline(state->md[*titleStart + *titleLength])) (*titleLength)++;

	if (*titleStart + *titleLength + 1 >= state->length ||
		state->md[*titleStart + *titleLength] != ']' ||
		state->md[*titleStart + *titleLength + 1] != '(') return 0;

	*linkStart = *titleStart + *titleLength + 2;
	*linkLength = 0;
	while(*linkStart + *linkLength < state->length && state->md[*linkStart + *linkLength] != ')'
		&& !mumd_internal_is_newline(state->md[*linkStart + *linkLength])) (*linkLength)++;

	if (state->md[*linkStart + *linkLength] != ')') return 0;

	return 1;
}

static inline void mumd_internal_pop_style(
	md_internal_state *state, int *text_start, int delim_length, int *style_index) {

	// commit text
	int text_length = state->i - *text_start;
	if (text_length>0) {
		state->cb((md_node){
			.type = md_text,
			.text = (char *)state->md + *text_start,
			.length = text_length});
	}


	// links and images may need to commit text that comes before them
	// in this case 0 if passed for delim_length, and the style remains unterminated
	if (delim_length) {
		state->cb((md_node){ .type = state->style_stack[--(*style_index)] + 1 });
		state->i+=delim_length;
		*text_start = state->i;
	}
}

// returns true if a delimiter is found on this line after `start`
// if a character in the delimeter is set to \\0 it is treates as "any except *"
// italics use "*\\0" since otherwise they would match on **
static inline int
mumd_internal_style_terminates(md_internal_state *state, int start, const char* delim, int delim_len) {
	for(int j=start;j<state->length-delim_len && !mumd_internal_is_newline(state->md[j]); j++) {
		int match = 1;
		for (int k=0;k<delim_len;k++) {
			if ((state->md[j+k] == delim[k] && delim[k] != 0) || (delim[k] == 0 && state->md[j+k] != '*')) {
				match=0;
				break;
			}
		}
		if (match) return 1;
	}
	return 0;
}

static inline void mumd_internal_push_style(md_internal_state *state, md_type style, int *text_start, int *style_index, const char* delim, int delim_len) {

	int terminates=mumd_internal_style_terminates(state, state->i+delim_len+1, delim, delim_len);

	if (style == md_italics_start) delim_len=1;

	if (terminates) {
		// commit text
		int text_end = state->i;
		state->cb((md_node){
			.type = md_text,
			.text = (char *)state->md + *text_start,
			.length = text_end - *text_start});

		// italics use "*\0", but should only increate i by 1

		// push style
		state->style_stack[(*style_index)++] = style;
		state->cb((md_node){ .type = style });
		state->i+=delim_len;
		*text_start=state->i;
	} else {
		state->i+=delim_len;
	}
}

static inline void
mumd_internal_parse_to_end_of_line(md_internal_state *state) {
	int titleStart, titleLength, linkStart, linkLength;
	int style_index=0;
	int text_start=state->i;


	while (state->i < state->length && !mumd_internal_is_newline(state->md[state->i])) {
		char c1 = state->md[state->i];
		char c2 = state->i+1 < state->length ? state->md[state->i+1] : 0;
		// todo: only calculate when push/pop happens
		md_type currentStyle = style_index ? state->style_stack[style_index-1] : md_text;

		switch (currentStyle)
		{
			case md_bold_start: {
				if (c1=='*' && c2 == '*') {
					mumd_internal_pop_style(state, &text_start, 2, &style_index);
					continue;
				}
			} break;
			case md_italics_start: {
				if (currentStyle == md_italics_start && c1=='*' && c2 != '*') {
					mumd_internal_pop_style(state, &text_start, 1, &style_index);
					continue;
				}
			} break;
			case md_code_start: {
				if (currentStyle == md_code_start && c1=='`') {
					mumd_internal_pop_style(state, &text_start, 1, &style_index);
					continue;
				}

			} break;
		}

		switch (c1)
		{
			case '*': {
				if (c2 == '*') mumd_internal_push_style(state, md_bold_start, &text_start, &style_index, "**", 2);
				else mumd_internal_push_style(state, md_italics_start, &text_start, &style_index, "*\0", 2);
			} break;
			case '`': mumd_internal_push_style(state, md_code_start, &text_start, &style_index, "`", 1); break;
			case '!': {
				// image
				state->i++;
				int validImage = mumd_internal_parse_link(state, &linkStart, &linkLength, &titleStart, &titleLength);
				if (validImage) {
					state->i--;
					mumd_internal_pop_style(state, &text_start, 0, &style_index);
					state->i++;

					state->i = linkStart + linkLength + 1;
					state->cb((md_node){ .type = md_image_start, .text = (char *)state->md + titleStart, .length = titleLength});
					state->cb((md_node){ .type = md_image_end, .text = (char *)state->md + linkStart, .length = linkLength});
					text_start=state->i;
				} else { /* no i++ since we just skip the !*/ }
			} break;
			case '[': {
				// link
				int validLink = mumd_internal_parse_link(state, &linkStart, &linkLength, &titleStart, &titleLength);
				if (validLink) {
					mumd_internal_pop_style(state, &text_start, 0, &style_index);

					state->i = linkStart + linkLength + 1;
					state->cb((md_node){ .type = md_link_start, .text = (char *)state->md + linkStart, .length = linkLength});
					state->cb((md_node){ .type = md_link_end, .text = (char *)state->md + titleStart, .length = titleLength});
					text_start=state->i;
				} else state->i++;
			} break;
			default: state->i++; break;
		} // end switch
	} // end while

	// commit the last text block
	mumd_internal_pop_style(state, &text_start, 0, &style_index);
}

int mumd_internal_match_list(md_internal_state *state) {
	return (state->i + 1 < state->length &&
		state->md[state->i] == '-' &&
		mumd_internal_is_whitespace(state->md[state->i+1]));
}

int mumd_internal_match_ordered_list(md_internal_state *state) {
	int j = 0;
	while (mumd_internal_is_number(state->md[state->i + j])) j++;
	if (state->length>state->i+j+1 &&
		state->md[state->i + j] == '.' &&
		mumd_internal_is_whitespace(state->md[state->i + j +1])
		) return j+2;

	return 0;
}

static inline void
mumd_internal_parse_to_end_of_block(md_internal_state *state) {
	while (state->i < state->length &&
		!mumd_internal_is_newline(state->md[state->i])) {
		mumd_internal_parse_to_end_of_line(state);
		if (state->md[state->i] == '\r') (state->i)++;
		if (state->md[state->i] == '\n') (state->i)++;
	}
}

static inline void mumd_internal_parse_separator(md_internal_state *state) {
	while (state->i < state->length &&
		mumd_internal_is_newline(state->md[state->i])) (state->i)++;
}

void mumd_parse(const char *md, int length, void (*cb)(const md_node node)) {

	md_internal_state state = {
		.md = md,
		.length = length,
		.cb = cb,
		.i = 0,
		.indent_level = 0
	};

	while (state.i < length)
	{
		mumd_internal_parse_separator(&state);

		int indentValue = 0;
		while(1) {
			if (md[state.i] == ' ') indentValue++;
			else if (md[state.i] == '\t') indentValue+=2;
			else break;
			state.i++;
		}

		if (state.indent_level) {

			md_internal_indent ind = state.indent_stack[state.indent_level-1];
			int nextIsListItem = (ind.type == md_unordered_list_start &&
					mumd_internal_match_list(&state)) ||
				(ind.type == md_ordered_list_start &&
					mumd_internal_match_ordered_list(&state));

			while (state.indent_level && (
				nextIsListItem && ind.spaces>indentValue ||
				!nextIsListItem && ind.spaces>=indentValue)) {

				cb((md_node){ .type = md_list_item_end });
				cb((md_node){
					 .type = state.indent_stack[state.indent_level-1].type+1 });
				state.indent_level--;

				if (state.indent_level) {
					ind = state.indent_stack[state.indent_level-1];
					nextIsListItem = (ind.type == md_unordered_list_start &&
						mumd_internal_match_list(&state)) ||
						(ind.type == md_ordered_list_start &&
							mumd_internal_match_ordered_list(&state));
				}
			}
		}

		switch (md[state.i])
		{
		case '#': {
			int h = 1;
			while (md[state.i + h] == '#') h++;

			if (md[state.i + h] != ' ' || h > 6) goto md_paragraph_jump;

			// skip #'s and space
			state.i += h + 1;

			int headOffset = (h - 1) * 2;
			cb((md_node){ .type = md_head_1_start + headOffset  });

			// parse text
			mumd_internal_parse_to_end_of_line(&state);

			cb((md_node){ .type = md_head_1_end + headOffset });

		} break;

		case '>': {
			if (state.i+1>=length) goto md_paragraph_jump;


			cb((md_node){ .type = md_quote_start });

			while(md[state.i] == '>') {
				// skip >
				state.i++;
				mumd_internal_parse_to_end_of_line(&state);
				if (md[state.i] == '\r') state.i++;
				if (md[state.i] == '\n') state.i++;
			}

			cb((md_node){ .type = md_quote_end });
		}

		case '`': {
			char c = md[state.i];
			int charcount = 0;
			while (md[state.i+charcount] == c) charcount++;

			if (charcount!=3) goto md_paragraph_jump;

			state.i += charcount;

			int j = 0;
			// find at least three
			while (md[state.i+j] != '\n' && md[state.i+j] != '\r') j++;

			cb((md_node){
				.type = md_code_block_start,
				.text = (char*)md + state.i,
				.length = j
			});

			state.i+=j;

			// parse lines until you reach another ```
			mumd_internal_parse_separator(&state);
			int code_start=state.i;
			while(1) {
				int before_trim = state.i;
				mumd_internal_parse_separator(&state);
				if (state.i+2 >= length) {
					state.i=before_trim;
					break;
				}
				if (md[state.i] == '`' &&
					md[state.i+1] == '`' &&
					md[state.i+2] == '`') {
						state.i=before_trim;
						break;
					}

				while (md[state.i] != '\n' && md[state.i] != '\r') state.i++;
			}
			int code_end=state.i;

			cb((md_node){
				.type = md_text,
				.text = (char*)md+code_start,
				.length = code_end-code_start
			});

			cb((md_node){ .type = md_code_block_end  });

			mumd_internal_parse_separator(&state);

			// skip the ```
			state.i+=3;
		} break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9': {
			int prefix = mumd_internal_match_ordered_list(&state);
			if (!prefix) goto md_paragraph_jump;

			state.i += prefix;

			int shouldIndent = state.indent_level < MUMD_MAX_NESTED_LISTS &&
				(state.indent_level==0 || indentValue -
					state.indent_stack[state.indent_level-1].spaces >= 2);

			if (shouldIndent) {
				state.indent_stack[state.indent_level++] =
					(md_internal_indent){ md_ordered_list_start, indentValue };

				cb((md_node){ .type = md_ordered_list_start });
			}

			if (!shouldIndent && state.indent_level>0) {
				// close the previous list item
				cb((md_node){ .type = md_list_item_end });
			}
			
			cb((md_node){ .type = md_list_item_start });
			mumd_internal_parse_to_end_of_line(&state);
		} break;

		case '+':
		case '*':
		case '-': {

			if (!mumd_internal_match_list(&state)) goto md_paragraph_jump;

			state.i += 2;

			int shouldIndent = state.indent_level < MUMD_MAX_NESTED_LISTS &&
				(state.indent_level==0 || indentValue -
					state.indent_stack[state.indent_level-1].spaces >= 2);

			if (shouldIndent) {
				state.indent_stack[state.indent_level++] = (md_internal_indent){
						md_unordered_list_start, indentValue };

				cb((md_node){ .type = md_unordered_list_start });
			}

			if (!shouldIndent && state.indent_level>0) {
				// close the previous list item
				cb((md_node){ .type = md_list_item_end });
			}
			
			cb((md_node){ .type = md_list_item_start });
			mumd_internal_parse_to_end_of_line(&state);
		} break;

		case '!': {
			state.i++;
			int titleStart, titleLength, linkStart, linkLength;
			int validImage = mumd_internal_parse_link(
				&state, &linkStart, &linkLength, &titleStart, &titleLength);

			if (!validImage) { state.i--; goto md_paragraph_jump; }

			state.i = linkStart + linkLength + 1;

			cb((md_node){ .type = md_image_start,
				.text = (char *)md + titleStart, .length = titleLength});

			cb((md_node){ .type = md_image_end,
				.text = (char *)md + linkStart, .length = linkLength});

		} break;

md_paragraph_jump:
		default: {
			// horizontal rule
			char c = md[state.i];
			if (c == '-' || c == '*' || c == '_') {
				int charcount = 0;
				// find at least three
				while (md[state.i+charcount] == c) charcount++;

				if (charcount>=3) {
					int j=state.i;
					while(md[j] == c || md[j] == ' ' || md[j] == '\t') j++;
					if (mumd_internal_is_newline(md[j])) {
						state.i = j;
						cb((md_node){ .type = md_horizontal_rule });
						break;
					}
				}
			}

			// paragraph
			cb((md_node){ .type = md_paragraph_start  });

			mumd_internal_parse_to_end_of_block(&state);

			cb((md_node){ .type = md_paragraph_end  });
		} break;
		}
	}
}

#endif // MUMD_IMPLEMENTATION
