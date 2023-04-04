


const char * const attr_type_names[] = {
	"none",
	"int",
	"float",
	"string",
	"wstring",
	"hash",
	"intarray",
	"floatarray",
	"filename",
	"id",
	"idref",
	"idhash",
	"idhashref"
};

int print_xml_tags(const void *rco_data, const void *element){

	const SceRcoHeader *pHeader = (const SceRcoHeader *)(rco_data);
	const SceRcoTreeHeader *element_header = (const SceRcoTreeHeader *)(element);

	for(int i=0;i<element_header->num_attributes;i++){
		void *base = (void *)(element + sizeof(SceRcoTreeHeader) + 0x10 * i);

		int attrname_handle = *(SceInt32 *)(base + 0);
		int attr_type = *(SceInt32 *)(base + 4);

		switch(attr_type){
		case attr_type_int:
			printf(" %s=\"%d\"", rco_dec_get_string(rco_data, attrname_handle), *(SceInt32 *)(base + 8));
			break;
		case attr_type_float:
			{
				float tmp;
				if(modff(*(float *)(base + 8), &tmp) == 0.0f){
					printf(" %s=\"%.0f\"", rco_dec_get_string(rco_data, attrname_handle), *(float *)(base + 8));
				}else{
					printf(" %s=\"%f\"", rco_dec_get_string(rco_data, attrname_handle), *(float *)(base + 8));
				}
			}

			break;
		case attr_type_string:
			printf(" %s=\"%s\"", rco_dec_get_string(rco_data, attrname_handle), rco_dec_get_string(rco_data, *(SceInt32 *)(base + 8)));
			break;
		case attr_type_wstring:
			// printf(" %s=\"%s\"", rco_dec_get_string(rco_data, attrname_handle), rco_dec_get_string(rco_data, *(SceInt32 *)(base + 8)));
			break;
		case attr_type_hash:
			if(*(SceInt32 *)(base + 0xC) != 4){
				exit(1);
			}

			{
				SceUInt32 *hashtable = (SceUInt32 *)(rco_data + pHeader->hashtable_offset);
				printf(" %s=\"0x%X\"", rco_dec_get_string(rco_data, attrname_handle), hashtable[*(SceInt32 *)(base + 8)]);
			}
			break;
		case attr_type_intarray:
			{
				SceInt32 *intarraytable = (SceInt32 *)(rco_data + pHeader->intarraytable_offset);
				int offset = *(SceInt32 *)(base + 8);
				int size = *(SceInt32 *)(base + 0xC);

				printf(" %s=\"", rco_dec_get_string(rco_data, attrname_handle));

				if(size != 0){
					printf("%d", intarraytable[offset + 0]);
					for(int i=1;i<size;i++){
						printf(", %d", intarraytable[offset + i]);
					}
				}

				printf("\"");
			}
			break;
		case attr_type_floatarray:
			// printf(" %s=\"0x%X, 0x%X\"", rco_dec_get_string(rco_data, attrname_handle), *(SceInt32 *)(base + 8), *(SceInt32 *)(base + 0xC));

			{
				float *floatarraytable = (float *)(rco_data + pHeader->floatarraytable_offset);
				int offset = *(SceInt32 *)(base + 8);
				int size = *(SceInt32 *)(base + 0xC);

				printf(" %s=\"", rco_dec_get_string(rco_data, attrname_handle));

				if(size != 0){
					float tmp;
					if(modff(floatarraytable[offset + 0], &tmp) == 0.0f){
						printf("%.0f", floatarraytable[offset + 0]);
					}else{
						printf("%f", floatarraytable[offset + 0]);
					}

					for(int i=1;i<size;i++){
						if(modff(floatarraytable[offset + i], &tmp) == 0.0f){
							printf(", %.0f", floatarraytable[offset + i]);
						}else{
							printf(", %f", floatarraytable[offset + i]);
						}
					}
				}

				printf("\"");
			}

			break;
		case attr_type_filename:
			printf(" %s=\"0x%X, 0x%X\"", rco_dec_get_string(rco_data, attrname_handle), *(SceInt32 *)(base + 8), *(SceInt32 *)(base + 0xC));
			break;
		case attr_type_id:
			printf(" %s=\"%s\"", rco_dec_get_string(rco_data, attrname_handle), (const char *)(rco_data + pHeader->idtable_offset + *(SceInt32 *)(base + 8) + 4));
			break;
		case attr_type_idref:
			printf(" %s=\"0x%X\"", rco_dec_get_string(rco_data, attrname_handle), *(SceInt32 *)(base + 8));
			break;
		case attr_type_idhash:
		case attr_type_idhashref:
			printf(" %s=\"0x%X\"", rco_dec_get_string(rco_data, attrname_handle), *(SceUInt32 *)(rco_data + pHeader->idhashtable_offset + *(SceInt32 *)(base + 8) + 4));
			break;
		default:
			break;
		}
	}

	return 0;
}

int print_xml(const void *rco_data, const void *element, int level){

	const SceRcoHeader *pHeader = (const SceRcoHeader *)(rco_data);
	const SceRcoTreeHeader *element_header = (const SceRcoTreeHeader *)(element);

	char *tab_data = malloc(level * 2 + 1);

	tab_data[level * 2] = 0;
	memset(tab_data, ' ', level * 2);

	if(element_header->first_child_elm_offset == -1 && element_header->last_child_elm_offset == -1){

		printf("%s<%s", tab_data, rco_dec_get_string(rco_data, element_header->name_handle));
		print_xml_tags(rco_data, element);
		printf(" />\n");

	}else if(element_header->first_child_elm_offset != -1){

		printf("%s<%s", tab_data, rco_dec_get_string(rco_data, element_header->name_handle));
		print_xml_tags(rco_data, element);
		printf(">\n");

		print_xml(rco_data, rco_data + pHeader->tree_offset + element_header->first_child_elm_offset, level + 1);
		printf("%s</%s>\n", tab_data, rco_dec_get_string(rco_data, element_header->name_handle));
	}

	free(tab_data);
	tab_data = NULL;

	if(element_header->next_elm_offset != -1){
		print_xml(rco_data, rco_data + pHeader->tree_offset + element_header->next_elm_offset, level);
	}

	return 0;
}
