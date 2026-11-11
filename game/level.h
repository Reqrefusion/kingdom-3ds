/* The compiled Ogmo level (fields.oel) as data. PlayState.buildLevel reads
   it the way the original reads the embedded XML. */
#ifndef TRANSLIT_LEVEL_H
#define TRANSLIT_LEVEL_H

typedef struct OelNode {
	const char *name; /* node.name(): the class to instantiate */
	int x, y;         /* node.@x, node.@y */
} OelNode;

typedef struct OelSection {
	const char *name;
	const OelNode *nodes;
	int count;
} OelSection;

typedef struct OelLevel {
	const char *backdropFarImg;
	const char *backdropCloseImg;
	int waterHeight;
	int groundTileWidth;
	int groundTileHeight;
	const char *ground;
	const OelSection *sections;
	int sectionCount;
} OelLevel;

extern const OelLevel Level_fields;

/* oel.<name> (null when the section is missing) */
static inline const OelSection *OelLevel_section(const OelLevel *level, const char *name)
{
	for (int i = 0; i < level->sectionCount; i++) {
		const char *a = level->sections[i].name, *b = name;
		while (*a && *a == *b) {
			a++;
			b++;
		}
		if (*a == *b)
			return &level->sections[i];
	}
	return 0;
}

#endif
