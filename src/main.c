

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <math.h>
#include <zlib.h>
#include "fs_list.h"


typedef int32_t SceInt32;
typedef uint32_t SceUInt32;
typedef unsigned short SceWChar16;


typedef struct SceRcoHeader { // size is 0x50-bytes
	char magic[4]; // CXML
	SceInt32 version;  // 0x110
	SceInt32 tree_offset;
	SceInt32 tree_size;

	SceInt32 idtable_offset;
	SceInt32 idtable_size;
	SceInt32 idhashtable_offset;
	SceInt32 idhashtable_size;

	// 0x20
	SceInt32 stringtable_offset;
	SceInt32 stringtable_size;
	SceInt32 wstringtable_offset;
	SceInt32 wstringtable_size;

	SceInt32 hashtable_offset;
	SceInt32 hashtable_size;
	SceInt32 intarraytable_offset;
	SceInt32 intarraytable_size;
	SceInt32 floatarraytable_offset;
	SceInt32 floatarraytable_size;
	SceInt32 filetable_offset;
	SceInt32 filetable_size;
} SceRcoHeader;

typedef struct SceRcoTreeHeader { // size is 0x1C-bytes
	SceInt32 name_handle;
	SceInt32 num_attributes;
	SceInt32 parent_elm_offset;
	SceInt32 prev_elm_offset;
	SceInt32 next_elm_offset;
	SceInt32 first_child_elm_offset;
	SceInt32 last_child_elm_offset;
} SceRcoTreeHeader;

#define attr_type_int        1
#define attr_type_float      2
#define attr_type_string     3
#define attr_type_wstring    4
#define attr_type_hash       5
#define attr_type_intarray   6
#define attr_type_floatarray 7
#define attr_type_filename   8
#define attr_type_id         9
#define attr_type_idref      10
#define attr_type_idhash     11
#define attr_type_idhashref  12


typedef struct CXmlTag CXmlTag;

typedef struct CXmlKeyValue {
	struct CXmlKeyValue *next;
	CXmlTag *tag;
	char *key;

	int type;
	union {
		struct {
			int data;
		} type_int;
		struct {
			float data;
		} type_float;
		struct {
			char *data;
		} type_string;
		struct {
			SceWChar16 *data;
		} type_wstring;
		struct {
			SceUInt32 data;
		} type_hash;
		struct {
			SceInt32 *data;
			int size;
		} type_intarray;
		struct {
			float *data;
			int size;
		} type_floatarray;
		struct {
			char *output;
			void *data;
			int size;
		} type_filename;
		struct {
			char *data;
		} type_id;
		struct {
			int unk; // TODO
		} type_idref;
		struct {
			SceUInt32 data;
		} type_idhash;
		struct {
			SceUInt32 data;
		} type_idhashref;
	};
} CXmlKeyValue;

typedef struct CXmlTag {
	struct CXmlTag *parent;
	struct CXmlTag *child;
	struct CXmlTag *next;
	char *name;
	CXmlKeyValue *kv;
} CXmlTag;


int create_file_with_recursive(const char *path, const void *data, int size){

	int path_len = strlen(path);
	char *new_path = malloc(path_len + 1);
	new_path[path_len] = 0;
	memcpy(new_path, path, path_len);

	char *x, *v = new_path;

	do {
		x = strchr(v, '/');
		if(x != NULL){

			*x = 0;

			struct stat stat_info;
			if(stat(new_path, &stat_info) != 0){
				mkdir(new_path, 0777);
				// printf("mkdir %s\n", v);
			}

			*x = '/';

			v = &(x[1]);
		}else if(strlen(v) != 0){
			// printf("create %s\n", v);

			FILE *fp;
			fp = fopen(path, "wb");
			if(fp == NULL){
				return -1;
			}

			fwrite(data, 1, (size_t)size, fp);
			fclose(fp);
			fp = NULL;
		}
	} while(x != NULL);

	free(new_path);
	new_path = NULL;

	return 0;
}

const char *rco_dec_get_string(const void *rco_data, int attr){

	const SceRcoHeader *pHeader;

	pHeader = (const SceRcoHeader *)rco_data;

	return (const char *)(rco_data + pHeader->stringtable_offset + attr);
}

const SceWChar16 *rco_dec_get_wstring(const void *rco_data, int attr){

	const SceRcoHeader *pHeader;

	pHeader = (const SceRcoHeader *)rco_data;

	return (const SceWChar16 *)(rco_data + pHeader->wstringtable_offset + attr);
}

int unicode2utf8(FILE *fp, SceWChar16 unicode){

	if(unicode < 0x80){

		if(unicode == '\n'){
			fprintf(fp, "&#xA;");
		}else{
			fprintf(fp, "%c", unicode);
		}

	}else if(unicode < 0x800){
		fprintf(fp, "%c", 0xC0 | ((unicode >> 6) & 0x1F));
		fprintf(fp, "%c", 0x80 | (unicode & 0x3F));
	}else{
		fprintf(fp, "%c", 0xE0 | ((unicode >> 12) & 0xF));
		fprintf(fp, "%c", 0x80 | ((unicode & 0xFC0) >> 6));
		fprintf(fp, "%c", 0x80 | (unicode & 0x3F));
	}

	return 0;
}

int sce_paf_wcslen(const SceWChar16 *wstr){

	const SceWChar16 *s = wstr;

	while(*wstr != 0){
		wstr++;
	}

	return wstr - s;
}

int sce_paf_fwprint(FILE *fp, const SceWChar16 *wstr){

	while(*wstr != 0){
		unicode2utf8(fp, *wstr);

		wstr++;
	}

	return 0;
}

int search_tag_key_by_name(CXmlTag *cxml, const char *name, CXmlKeyValue **result){

	CXmlKeyValue *kv = cxml->kv;

	while(kv != NULL){
		if(strcmp(kv->key, name) == 0){
			*result = kv;
			return 0;
		}

		kv = kv->next;
	}

	return -1;
}

int search_tag_child_by_name(CXmlTag *cxml, const char *name, CXmlTag **result){

	CXmlTag *child = cxml->child;

	while(child != NULL){
		if(strcmp(child->name, name) == 0){
			*result = child;
			return 0;
		}

		child = child->next;
	}

	return -1;
}

int parse_element_tags(const void *rco_data, const void *element, CXmlTag *tag, CXmlKeyValue **result){

	const SceRcoHeader *pHeader = (const SceRcoHeader *)(rco_data);
	const SceRcoTreeHeader *element_header = (const SceRcoTreeHeader *)(element);

	CXmlKeyValue *head, *tail;

	head = malloc(sizeof(*head));
	if(head == NULL){
		printf("%s: cannot alloc head\n", __FUNCTION__);
		return -1;
	}

	memset(head, 0, sizeof(*head));
	head->next = NULL;
	head->tag  = NULL;
	head->key  = NULL;

	*result = head;
	tail = head;

	for(int i=0;i<element_header->num_attributes;i++){

		void *base = (void *)(element + sizeof(SceRcoTreeHeader) + 0x10 * i);

		int attrname_handle = *(SceInt32 *)(base + 0);
		int attr_type = *(SceInt32 *)(base + 4);

		head = malloc(sizeof(*head));
		if(head == NULL){
			printf("%s: cannot alloc head\n", __FUNCTION__);
			return -1;
		}

		memset(head, 0, sizeof(*head));
		head->next = NULL;
		head->tag  = tag;
		head->key  = NULL;

		tail->next = head;
		tail = head;

		// init key
		{
			const char *key = rco_dec_get_string(rco_data, attrname_handle);
			int key_len =  strlen(key);

			char *new_key = malloc(key_len + 1);

			new_key[key_len] = 0;
			memcpy(new_key, key, key_len);
			head->key = new_key;
		}

		head->type = attr_type;

		// printf("A %s\n", head->key);

		switch(attr_type){
		case attr_type_int:
			head->type_int.data = *(SceInt32 *)(base + 8);
			break;
		case attr_type_float:
			head->type_float.data = *(float *)(base + 8);
			break;
		case attr_type_string:
			{
				const char *str = rco_dec_get_string(rco_data, *(SceInt32 *)(base + 8));
				int str_len = strlen(str);
				char *new_str = malloc(str_len + 1);
				new_str[str_len] = 0;
				memcpy(new_str, str, str_len);

				head->type_string.data = new_str;
			}
			break;
		case attr_type_wstring:
			{
				const SceWChar16 *wstr = rco_dec_get_wstring(rco_data, *(SceInt32 *)(base + 8) << 1);
				int wstr_len = sce_paf_wcslen(wstr);
				SceWChar16 *new_wstr = malloc((wstr_len + 1) * 2);
				new_wstr[wstr_len] = 0;
				memcpy(new_wstr, wstr, wstr_len * 2);

				head->type_wstring.data = new_wstr;
			}
			break;
		case attr_type_hash:
			if(*(SceInt32 *)(base + 0xC) != 4){
				exit(1);
			}

			{
				SceUInt32 *hashtable = (SceUInt32 *)(rco_data + pHeader->hashtable_offset);
				head->type_hash.data = hashtable[*(SceInt32 *)(base + 8)];
			}
			break;
		case attr_type_intarray:
			{
				SceInt32 *intarraytable = (SceInt32 *)(rco_data + pHeader->intarraytable_offset);
				int offset = *(SceInt32 *)(base + 8);
				int size = *(SceInt32 *)(base + 0xC);

				SceInt32 *new_intarray = malloc(sizeof(SceInt32) * size);
				memcpy(new_intarray, &(intarraytable[offset]), sizeof(SceInt32) * size);

				head->type_intarray.data = new_intarray;
				head->type_intarray.size = size;
			}
			break;
		case attr_type_floatarray:
			// printf(" %s=\"0x%X, 0x%X\"", rco_dec_get_string(rco_data, attrname_handle), *(SceInt32 *)(base + 8), *(SceInt32 *)(base + 0xC));

			{
				float *floatarraytable = (float *)(rco_data + pHeader->floatarraytable_offset);
				int offset = *(SceInt32 *)(base + 8);
				int size = *(SceInt32 *)(base + 0xC);

				float *new_floatarray = malloc(sizeof(float) * size);
				memcpy(new_floatarray, &(floatarraytable[offset]), sizeof(float) * size);

				head->type_floatarray.data = new_floatarray;
				head->type_floatarray.size = size;
			}

			break;
		case attr_type_filename:
			{
				const void *filetable = (const void *)(rco_data + pHeader->filetable_offset);
				int offset = *(SceInt32 *)(base + 8);
				int size = *(SceInt32 *)(base + 0xC);

				void *new_file = malloc(size);
				memcpy(new_file, filetable + offset, size);

				head->type_filename.data = new_file;
				head->type_filename.size = size;
			}
			break;
		case attr_type_id:
			{
				const char *id = (const char *)(rco_data + pHeader->idtable_offset + *(SceInt32 *)(base + 8) + 4);
				int id_len = strlen(id);
				char *new_id = malloc(id_len + 1);
				new_id[id_len] = 0;
				memcpy(new_id, id, id_len);

				head->type_id.data = new_id;
			}
			break;
		case attr_type_idref:
			// printf(" %s=\"0x%X\"", rco_dec_get_string(rco_data, attrname_handle), *(SceInt32 *)(base + 8));
			break;
		case attr_type_idhash:
			head->type_idhash.data = *(SceUInt32 *)(rco_data + pHeader->idhashtable_offset + *(SceInt32 *)(base + 8) + 4);
			break;
		case attr_type_idhashref:
			head->type_idhashref.data = *(SceUInt32 *)(rco_data + pHeader->idhashtable_offset + *(SceInt32 *)(base + 8) + 4);
			break;
		default:
			break;
		}

		// printf("B %s\n", head->key);
	}

	head = *result;

	*result = head->next;
	free(head);
	head = NULL;

	return 0;
}

int parse_element(const void *rco_data, const void *element, CXmlTag *parent, CXmlTag **result){

	const SceRcoHeader *pHeader = (const SceRcoHeader *)(rco_data);
	const SceRcoTreeHeader *element_header = (const SceRcoTreeHeader *)(element);

	CXmlTag *cxml;

	cxml = malloc(sizeof(*cxml));
	if(cxml == NULL){
		return -1;
	}

	*result = cxml;

	memset(cxml, 0, sizeof(*cxml));
	cxml->parent = parent;
	cxml->child  = NULL;
	cxml->next   = NULL;
	cxml->name   = NULL;
	cxml->kv     = NULL;

	const char *name = rco_dec_get_string(rco_data, element_header->name_handle);
	int name_len = strlen(name);

	char *new_name = malloc(name_len + 1);

	new_name[name_len] = 0;
	memcpy(new_name, name, name_len);

	cxml->name = new_name;

	if(element_header->first_child_elm_offset == -1 && element_header->last_child_elm_offset == -1){

		parse_element_tags(rco_data, element, cxml, &(cxml->kv));

	}else if(element_header->first_child_elm_offset != -1){

		parse_element_tags(rco_data, element, cxml, &(cxml->kv));
		parse_element(rco_data, rco_data + pHeader->tree_offset + element_header->first_child_elm_offset, cxml, &(cxml->child));

		// cxml->child->parent = cxml;
	}

	if(element_header->next_elm_offset != -1){
		parse_element(rco_data, rco_data + pHeader->tree_offset + element_header->next_elm_offset, cxml->parent, &(cxml->next));

		// cxml->next->parent = cxml->parent;
	}

	return 0;
}

int print_cxml_tags(const char *output_path, FILE *xml_fp, CXmlKeyValue *kv){

	while(kv != NULL){

		fprintf(xml_fp, " %s=\"", kv->key);

		switch(kv->type){
		case attr_type_int:
			fprintf(xml_fp, "%d", kv->type_int.data);
			break;
		case attr_type_float:
			{
				float tmp;
				if(modff(kv->type_float.data, &tmp) == 0.0f){
					// printf("%.0f", kv->type_float.data);
				}else{
					// printf("%f", kv->type_float.data);
				}

				fprintf(xml_fp, "%g", kv->type_float.data);
			}

			break;
		case attr_type_string:
			fprintf(xml_fp, "%s", kv->type_string.data);
			break;
		case attr_type_wstring:
			sce_paf_fwprint(xml_fp, kv->type_wstring.data);
			break;
		case attr_type_hash:
			fprintf(xml_fp, "0x%08X", kv->type_hash.data);
			break;
		case attr_type_intarray:
			{
				if(kv->type_intarray.size != 0){
					fprintf(xml_fp, "%d", kv->type_intarray.data[0]);
					for(int i=1;i<kv->type_intarray.size;i++){
						fprintf(xml_fp, ", %d", kv->type_intarray.data[i]);
					}
				}
			}
			break;
		case attr_type_floatarray:
			{
				if(kv->type_floatarray.size != 0){
					float tmp;
					if(modff(kv->type_floatarray.data[0], &tmp) == 0.0f){
						// printf("%.0f", kv->type_floatarray.data[0]);
					}else{
						// printf("%f", kv->type_floatarray.data[0]);
					}

					fprintf(xml_fp, "%g", kv->type_floatarray.data[0]);

					for(int i=1;i<kv->type_floatarray.size;i++){
						if(modff(kv->type_floatarray.data[i], &tmp) == 0.0f){
							// printf(", %.0f", kv->type_floatarray.data[i]);
						}else{
							// printf(", %f", kv->type_floatarray.data[i]);
						}

						fprintf(xml_fp, ", %g", kv->type_floatarray.data[i]);
					}
				}
			}

			break;
		case attr_type_filename:
			{
				char src_path[0x80];
				CXmlTag *tag = kv->tag;

				if(strcmp(tag->name, "locale") == 0){
					CXmlKeyValue *kv_id;
					search_tag_key_by_name(tag, "id", &kv_id);
					snprintf(src_path, sizeof(src_path), "%s/locale/plugin_locale_%s.xml.rcs", output_path, kv_id->type_id.data);
				}else if(strcmp(tag->name, "texture") == 0 || strcmp(tag->name, "file") == 0 || strcmp(tag->name, "sounddata") == 0){
					CXmlKeyValue *kv_id, *kv_type;
					search_tag_key_by_name(tag, "id", &kv_id);
					search_tag_key_by_name(tag, "type", &kv_type);

					const char *type = kv_type->type_string.data;

					const char *part = strchr(type, '/');
					if(part != NULL){
						char *new_name = malloc((part - type) + 1);
						new_name[part - type] = 0;
						memcpy(new_name, type, part - type);
						snprintf(src_path, sizeof(src_path), "%s/%s/%s_0x%08X.%s", output_path, tag->name, new_name, kv_id->type_int.data, &(part[1]));
						free(new_name);
						new_name = NULL;
					}else{
						snprintf(src_path, sizeof(src_path), "%s/%s/%s_0x%08X.tex", output_path, tag->name, tag->name, kv_id->type_int.data);
					}

				}else{
					src_path[0] = 0;
				}

				CXmlKeyValue *kv_compress = NULL;
				search_tag_key_by_name(tag, "compress", &kv_compress);

				if(kv_compress != NULL && strcmp(kv_compress->type_string.data, "on") == 0){

					CXmlKeyValue *kv_origsize = NULL;
					search_tag_key_by_name(tag, "origsize", &kv_origsize);

					long unsigned int temp_size = kv_origsize->type_int.data;
					void *temp_memory_ptr = malloc(temp_size);

					int res = uncompress(temp_memory_ptr, &temp_size, kv->type_filename.data, kv->type_filename.size);
					if(res != Z_OK){
						printf("zlib uncompress failed : 0x%X\n", res);
					}

					create_file_with_recursive(src_path, temp_memory_ptr, kv_origsize->type_int.data);

					free(temp_memory_ptr);
					temp_memory_ptr = NULL;

				}else{
					create_file_with_recursive(src_path, kv->type_filename.data, kv->type_filename.size);
				}


				int src_len = strlen(src_path);
				char *new_src = malloc(src_len + 1);
				new_src[src_len] = 0;
				memcpy(new_src, src_path, src_len);

				kv->type_filename.output = new_src;

				fprintf(xml_fp, "%s", new_src + strlen(output_path) + 1);
			}
			break;
		case attr_type_id:
			fprintf(xml_fp, "%s", kv->type_id.data);
			break;
		case attr_type_idref:
			// TODO
			break;
		case attr_type_idhash:
			fprintf(xml_fp, "0x%08X", kv->type_idhash.data);
			break;
		case attr_type_idhashref:
			fprintf(xml_fp, "0x%08X", kv->type_idhashref.data);
			break;
		default:
			break;
		}

		fprintf(xml_fp, "\"");

		kv = kv->next;
	}

	return 0;
}

int print_cxml(const char *output_path, FILE *xml_fp, CXmlTag *cxml, int level){

	int res;

	char *tab_data = malloc(level * 2 + 1);
	if(tab_data == NULL){
		return -1;
	}

	tab_data[level * 2] = 0;
	memset(tab_data, ' ', level * 2);

	if(cxml->child == NULL){

		fprintf(xml_fp, "%s<%s", tab_data, cxml->name);
		print_cxml_tags(output_path, xml_fp, cxml->kv);
		fprintf(xml_fp, " />\n");

	}else if(cxml->child != NULL){

		fprintf(xml_fp, "%s<%s", tab_data, cxml->name);
		print_cxml_tags(output_path, xml_fp, cxml->kv);
		fprintf(xml_fp, ">\n");

		res = print_cxml(output_path, xml_fp, cxml->child, level + 1);
		if(res < 0){
			return res;
		}

		fprintf(xml_fp, "%s</%s>\n", tab_data, cxml->name);

	}

	free(tab_data);
	tab_data = NULL;

	if(cxml->next != NULL){
		res = print_cxml(output_path, xml_fp, cxml->next, level);
		if(res < 0){
			return res;
		}
	}

	return 0;
}

int free_cxml_tags(CXmlKeyValue *kv){

	CXmlKeyValue *kv_next;

	while(kv != NULL){

		kv_next = kv->next;

		// printf("%s\n", kv->key);

		switch(kv->type){
		case attr_type_int:
			break;
		case attr_type_float:
			break;
		case attr_type_string:
			free(kv->type_string.data);
			break;
		case attr_type_wstring:
			free(kv->type_wstring.data);
			break;
		case attr_type_hash:
			break;
		case attr_type_intarray:
			free(kv->type_intarray.data);
			break;
		case attr_type_floatarray:
			free(kv->type_floatarray.data);
			break;
		case attr_type_filename:
			free(kv->type_filename.output);
			free(kv->type_filename.data);
			break;
		case attr_type_id:
			free(kv->type_id.data);
			break;
		case attr_type_idref:
			// TODO
			break;
		case attr_type_idhash:
			break;
		case attr_type_idhashref:
			break;
		default:
			break;
		}

		// printf("B\n");

		free(kv->key);
		free(kv);

		kv = kv_next;
	}

	return 0;
}

int free_cxml(CXmlTag *cxml){

	if(cxml->child == NULL){
		free_cxml_tags(cxml->kv);
	}else if(cxml->child != NULL){
		free_cxml_tags(cxml->kv);
		free_cxml(cxml->child);
	}

	if(cxml->next != NULL){
		free_cxml(cxml->next);
	}

	free(cxml);

	return 0;
}

int RcsDecompiler_core(const char *xml_name, const void *rcs_data, int rcs_size){

	const SceRcoHeader *pHeader;
	CXmlTag *result;
	FILE *xml_fp;

	pHeader = (const SceRcoHeader *)rcs_data;

	if(memcmp(pHeader->magic, "RCSF", 4) != 0){
		printf("Header bad magic (%02X %02X %02X %02X)\n", pHeader->magic[0], pHeader->magic[1], pHeader->magic[2], pHeader->magic[3]);
		return -1;
	}

	xml_fp = fopen(xml_name, "wb");
	if(xml_fp == NULL){
		return -1;
	}

	fprintf(xml_fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	// fprintf(xml_fp, "<?xml version=\"1.0\" encoding=\"unicode\"?>\n");

	parse_element(rcs_data, (const void *)(rcs_data + pHeader->tree_offset), NULL, &result);
	print_cxml("NULL", xml_fp, result, 0);
	free_cxml(result);
	result = NULL;

	fclose(xml_fp);
	xml_fp = NULL;

	return 0;
}

int RcsDecompiler(const char *xml_name, const char *path, int flags){

	int res;
	long length;
	char *rcs_data = NULL;

	FILE *fp;
	fp = fopen(path, "rb");
	if(fp == NULL){
		return -1;
	}

	do {
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);

		rcs_data = malloc(length);
		if(rcs_data == NULL){
			res = -1;
			break;
		}

		fseek(fp, 0, SEEK_SET);
		fread(rcs_data, 1, (size_t)length, fp);

		fclose(fp);
		fp = NULL;

		res = RcsDecompiler_core(xml_name, rcs_data, length);

		free(rcs_data);
		rcs_data = NULL;
	} while(0);

	return res;
}

int process_stringtable(CXmlTag *root){

	CXmlTag *cxml_stringtable_tag = NULL;

	search_tag_child_by_name(root, "stringtable", &cxml_stringtable_tag);

	if(cxml_stringtable_tag != NULL){
		printf("Found %s in %s\n", cxml_stringtable_tag->name, cxml_stringtable_tag->parent->name);

		CXmlTag *locale_link = cxml_stringtable_tag->child;
		while(locale_link != NULL){
			// printf("%s\n", locale_link->name);

			if(strcmp(locale_link->name, "locale") == 0){

				CXmlKeyValue *kv_src = NULL;
				search_tag_key_by_name(locale_link, "src", &kv_src);

				printf("%s\n", kv_src->type_filename.output);
			}
			locale_link = locale_link->next;
		}
	}

	return 0;
}

int xml_list_callback(FSListEntry *ent, void *argp){
	// printf("%p %s\n", ent, ent->path_full);


	int path_len = strlen(ent->path_full);
	char *new_path = malloc(path_len + 1);
	new_path[path_len] = 0;
	memcpy(new_path, ent->path_full, path_len);

	char *x = strrchr(new_path, '.');
	*x = 0;

	// printf("%s\n", new_path);

	RcsDecompiler(new_path, ent->path_full, 0);

	free(new_path);
	new_path = NULL;

	return 0;
}

int RcoDecompiler_core(const char *plugin_name, const void *rco_data, int rco_size){

	const SceRcoHeader *pHeader;
	CXmlTag *result;
	char xml_name[0x40];
	FILE *xml_fp;


	pHeader = (const SceRcoHeader *)rco_data;

	if(memcmp(pHeader->magic, "RCOF", 4) != 0){
		printf("Header bad magic (%02X %02X %02X %02X)\n", pHeader->magic[0], pHeader->magic[1], pHeader->magic[2], pHeader->magic[3]);
		return -1;
	}

	snprintf(xml_name, sizeof(xml_name), "%s/", plugin_name);

	create_file_with_recursive(xml_name, NULL, 0);

	snprintf(xml_name, sizeof(xml_name), "%s/%s.xml", plugin_name, plugin_name);


	xml_fp = fopen(xml_name, "wb");
	if(xml_fp == NULL){
		return -1;
	}

	fprintf(xml_fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");

	parse_element(rco_data, (const void *)(rco_data + pHeader->tree_offset), NULL, &result);
	print_cxml(plugin_name, xml_fp, result, 0);

	// TODO: Properly handle it here instead of inside print_cxml.
	// process_stringtable(result);

	free_cxml(result);
	result = NULL;

	fclose(xml_fp);
	xml_fp = NULL;

	{
		FSListEntry *ent = NULL;

		snprintf(xml_name, sizeof(xml_name), "%s/locale", plugin_name);

		int res = fs_list_init(xml_name, &ent, NULL, NULL);
		if(res >= 0){
			fs_list_execute(ent->child, xml_list_callback, NULL);
		}
		fs_list_fini(ent);
	}

	return 0;
}

int RcoDecompiler(const char *path, int flags){

	int res;
	long length;
	char *rco_data = NULL;

	FILE *fp;
	fp = fopen(path, "rb");
	if(fp == NULL){
		return -1;
	}

	do {
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);

		rco_data = malloc(length);
		if(rco_data == NULL){
			res = -1;
			break;
		}

		fseek(fp, 0, SEEK_SET);
		fread(rco_data, 1, (size_t)length, fp);

		fclose(fp);
		fp = NULL;

		{
			const char *name = strrchr(path, '/');
			if(name != NULL){
				name = &(name[1]);
			}else{
				name = path;
			}

			const char *name_c = strrchr(name, '.');
			if(name_c != NULL){
				int name_len = name_c - name;
				char *new_name = malloc(name_len + 1);
				new_name[name_len] = 0;
				memcpy(new_name, name, name_len);
				res = RcoDecompiler_core(new_name, rco_data, length);
				free(new_name);
				new_name = NULL;
			}else{
				res = RcoDecompiler_core(name, rco_data, length);
			}
		}

		free(rco_data);
		rco_data = NULL;
	} while(0);

	return res;
}

int main(int argc, char *argv[]){

	RcoDecompiler(argv[1], 0);

	return 0;
}
