
struct menuboxbounds {
	unsigned char		cols,rows;
	unsigned char		ofsx,ofsy;
	unsigned char		col_width;
	unsigned char		width,height;
	const char**		item_string;
	int			item_max;
};

void menuboxbound_redraw(struct menuboxbounds *mbox,int select);
void menuboxbounds_set_def_list(struct menuboxbounds *mbox,unsigned int ofsx,unsigned int ofsy,unsigned int cols);
void menuboxbounds_set_item_strings_array(struct menuboxbounds *mbox,const char **str,unsigned int count);

#define menuboxbounds_set_item_strings_arraylen(mbox,str) \
	menuboxbounds_set_item_strings_array(mbox,str,sizeof(str)/sizeof(str[0]))

