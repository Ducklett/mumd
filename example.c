#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MUMD_IMPLEMENTATION
#include "./mumd.h"

void node_parsed(const md_node node) {
	switch(node.type) {
		case md_text:             printf("%.*s ", node.length, node.text); break;
		case md_code_start:       printf("<code>"); break;
		case md_code_end:         printf("</code>"); break;
		case md_bold_start:       printf("<strong>"); break;
		case md_bold_end:         printf("</strong>"); break;
		case md_italics_start:    printf("<em>"); break;
		case md_italics_end:      printf("</em>"); break;
		case md_strikethrough_start: printf("<del>"); break;
		case md_strikethrough_end:   printf("</del>"); break;

		case md_link_start:       printf("<a href=\"%.*s\">", node.length, node.text); break;
		case md_link_end:         printf("%.*s</a>\n", node.length, node.text); break;
		case md_image_start:      printf("<img alt=\"%.*s\"", node.length, node.text); break;
		case md_image_end:        printf(" src=\"%.*s\">\n", node.length, node.text); break;

		case md_head_1_start:     printf("<h1>"); break;
		case md_head_1_end:       printf("</h1\n>"); break;
		case md_head_2_start:     printf("<h2>"); break;
		case md_head_2_end:       printf("</h2>\n"); break;
		case md_head_3_start:     printf("<h3>"); break;
		case md_head_3_end:       printf("</h3>\n"); break;
		case md_head_4_start:     printf("<h4>"); break;
		case md_head_4_end:       printf("</h4>\n"); break;
		case md_head_5_start:     printf("<h5>"); break;
		case md_head_5_end:       printf("</h5>\n"); break;
		case md_head_6_start:     printf("<h6>"); break;
		case md_head_6_end:       printf("</h6>\n"); break;

		case md_paragraph_start:  printf("<p>"); break;
		case md_paragraph_end:    printf("</p>\n"); break;

		case md_code_block_start: printf("<pre class=\"lang-%.*s\"><code>", node.length, node.text); break;
		case md_code_block_end:   printf("</code></pre>\n"); break;

		case md_quote_start:      printf("<blockquote>"); break;
		case md_quote_end:        printf("</blockquote>\n"); break;

		case md_unordered_list_start: printf("<ul>"); break;
		case md_unordered_list_end:   printf("</ul>\n"); break;
		case md_ordered_list_start:   printf("<ol>"); break;
		case md_ordered_list_end:     printf("</ol>\n"); break;
		case md_list_item_start:      printf("<li>"); break;
		case md_list_item_end:        printf("</li>\n"); break;

		case md_horizontal_rule:      printf("<hr>\n"); break;
	}
}

void help() {
	fprintf(stderr, "usage: mumd <filename>\n");
	fprintf(stderr, "-s\tread from stdin instead of file\n");
	exit(1);
}

void main(int argc, const char** argv) {

	if (argc<=1) help();
	if (strcmp(argv[1], "-s") == 0) {

		char c;
		size_t length = 0;
		size_t capacity = sizeof(char) * 512;
		char* buff = malloc(capacity);

		while((c = getchar()) != EOF && c != '\0') {
			buff[length++] = c;
			if (length==capacity) {
				char* oldBuff = buff;
				buff = malloc(capacity*2);
				memcpy(buff, oldBuff,capacity);
				capacity*=2;
				free(oldBuff);
			}
		}

		if (length == 0) help();

		mumd_parse(buff, length, node_parsed);
	} else {
		char * buff = 0;
		long length;

		FILE * f = fopen (argv[1], "rb");

		if (f) {
  			fseek (f, 0, SEEK_END);
  			length = ftell (f);
  			fseek (f, 0, SEEK_SET);
  			buff = malloc (length);
  			if (buff) {
    			fread (buff, 1, length, f);
  			}
  			fclose (f);
		}

		if (!buff) {
			fprintf(stderr, "failed to open file %s", argv[1]);
			exit(1);
		}

		mumd_parse(buff, length, node_parsed);
	}

}
