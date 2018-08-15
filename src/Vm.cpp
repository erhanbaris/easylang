#include "Vm.h"
#include "Console.h"
#include <unordered_map>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef _WIN32
# include <windows.h>
#else
# include <sys/time.h>
# include <unistd.h>
#endif

#define PRINT_ACTIVE 0
#define PERFORMANCE_MODE 0

#if PRINT_ACTIVE == 1
#define PRINT_OPCODE() console_out << _T(" > ") << vm_instToString((vm_inst)*code) << '\n';
#define PRINT_STACK() console_out << _T("STACK: ") << stackIndex << _T(" " ) << __LINE__ << '\n';
#define PRINT_AND_CHECK_STACK() PRINT_STACK(); if (stackIndex == 0) { dumpOpcode(startPoint, len); dump(startPoint, len); __asm { int 3 } }
#else
#define PRINT_OPCODE() 
#define PRINT_STACK() 
#define PRINT_AND_CHECK_STACK()
#endif

#if PERFORMANCE_MODE == 0
#define FUNC_BEGIN() [&]{
#define FUNC_END() }();
#else 
#define FUNC_BEGIN()
#define FUNC_END()
#endif

#define STORE(index, obj) FUNC_BEGIN() *(currentStore->variables + (index)) = obj; FUNC_END()
#define LOAD(index) ((vm_object) currentStore[ index ])

#define GSTORE(index, obj) FUNC_BEGIN() *(globalStore.variables + (index)) = obj; FUNC_END()
#define GLOAD(index) (globalStore.variables[ index ])


#define PEEK_INDEX(index) (currentStack[stackIndex - index ])
#define PEEK_INDEX_PTR(index) (&currentStack[stackIndex - index ])

#define PEEK() (PEEK_INDEX(1))
#define PEEK_PTR() (PEEK_INDEX_PTR(1))
#define POP() (currentStack[--stackIndex])
#define POP_VALUE() (currentStack[--stackIndex])
#define POP_AS(type) AS_BOOL(currentStack[--stackIndex])
#define SET(obj) currentStack[stackIndex - 1] = obj

#define OPERATION(currentStack, stackIndex, op, type) currentStack [ stackIndex - 2]. type = currentStack [ stackIndex - 2]. type ##op currentStack [ stackIndex - 1]. type ;\
    PRINT_AND_CHECK_STACK();\
    -- stackIndex ;

#define PUSH_WITH_STACK(currentStack, stackIndex, type, obj) FUNC_BEGIN() currentStack [ stackIndex ]. type = obj . type ; PRINT_STACK(); ++ stackIndex; FUNC_END()
#define PUSH_WITH_INIT(obj) currentStack [ stackIndex ] = GET_VALUE_FROM_OBJ(new vm_object( obj )) ; PRINT_STACK(); ++ stackIndex;
#define PUSH_WITH_EMPTY() currentStack [ stackIndex ] = 0 ; PRINT_STACK(); ++ stackIndex;
#define PUSH(type, obj) currentStack [ stackIndex ]. type = obj ; PRINT_STACK(); ++ stackIndex;
#define PUSH_WITH_ASSIGN(obj) currentStack [ stackIndex ] = obj ; PRINT_STACK(); ++ stackIndex;
#define PUSH_WITH_ASSIGN_INT(obj) currentStack [ stackIndex ] = numberToValue( obj ) ; PRINT_STACK(); ++ stackIndex;
#define INC(currentStack, stackIndex, type) currentStack [ stackIndex - 1]. type = currentStack [ stackIndex - 1]. type + 1;
#define DINC(currentStack, stackIndex, type) currentStack [ stackIndex - 1]. type = currentStack [ stackIndex - 1]. type - 1;
#define NEG(currentStack, stackIndex, type) currentStack [ stackIndex - 1]. type =  currentStack [ stackIndex - 1]. type * -1;
#define LOAD_AND_PUSH(index) FUNC_BEGIN() { currentStack[stackIndex] = *(currentStore->variables + index );\
    PRINT_STACK(); \
    ++ stackIndex; } FUNC_END()

#define STACK_DINC() PRINT_AND_CHECK_STACK(); -- stackIndex ;
#define GET_ITEM(index, type) currentStack[stackIndex - index ]. type
#define GET_VALUE(index) currentStack[stackIndex - index ]
#define OPERATION_ADD(var, type) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, var) +  GET_ITEM(1, var); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_SUB(var, type) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, var) -  GET_ITEM(1, var); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_MUL(var, type) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, var) *  GET_ITEM(1, var); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_DIV(var, type) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, var) /  GET_ITEM(1, var); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_EQ( var, type, condType) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, condType) == GET_ITEM(1, condType); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_LT( var, type, condType) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, condType) <  GET_ITEM(1, condType); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_LTE(var, type, condType) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, condType) <= GET_ITEM(1, condType); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_GT( var, type, condType) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, condType) >  GET_ITEM(1, condType); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_GTE(var, type, condType) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, condType) >= GET_ITEM(1, condType); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_AND(var, type) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, var) && GET_ITEM(1, var); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define OPERATION_OR( var, type) FUNC_BEGIN() GET_ITEM(2, var) = GET_ITEM(2, var) || GET_ITEM(1, var); GET_ITEM(2, Type) = type; STACK_DINC() FUNC_END()
#define IS_EQUAL(condType) GET_ITEM(2, condType) == GET_ITEM(1, condType)
#define CONVERT(from, to, toType) currentStack[stackIndex - 1]. to = currentStack[stackIndex - 1]. from ; currentStack[stackIndex - 1].Type = toType ;

#define GLOAD_PRE(index) opt_GLOAD_##index:\
++code;\
PRINT_OPCODE();\
PUSH_WITH_ASSIGN(GLOAD( index ));\
GOTO_OPCODE();

#define GSTORE_PRE(index) opt_GSTORE_##index:\
++code;\
PRINT_OPCODE();\
GSTORE( index , POP());\
GOTO_OPCODE();

#define LOAD_PRE(index) opt_LOAD_##index:\
++code;\
PRINT_OPCODE();\
LOAD_AND_PUSH( index );\
GOTO_OPCODE();

#define STORE_PRE(index) opt_STORE_##index:\
++code;\
PRINT_OPCODE();\
STORE( index , POP());\
GOTO_OPCODE();



#define ASSIGN_8(data, code)\
    data [7] = *( code + 0);\
    data [6] = *( code + 1);\
    data [5] = *( code + 2);\
    data [4] = *( code + 3);\
    data [3] = *( code + 4);\
    data [2] = *( code + 5);\
    data [1] = *( code + 6);\
    data [0] = *( code + 7);\
    code += 8;

#define ASSIGN_4(data, code)\
    data [3] = *( code + 0);\
    data [2] = *( code + 1);\
    data [1] = *( code + 2);\
    data [0] = *( code + 3);\
    code += 4;

#define ASSIGN_3(data, code)\
    data [2] = *( code + 0);\
    data [1] = *( code + 1);\
    data [0] = *( code + 2);\
    code += 3;

#define ASSIGN_2(data, code)\
    data [1] = *( code + 0);\
    data [0] = *( code + 1);\
    code += 2;

#define ASSIGN_1(data, code)\
    data [0] = *code++;

#ifdef _WIN32
#  define STORE_ADDRESS(index,label) __asm lea eax, label __asm mov edx,testCodes\
     __asm mov [edx][index * TYPE testCodes],eax
#  define GOTO_OPCODE() { void* addr = gotoAddresses[*code]; __asm jmp addr }

#else
#  define STORE_ADDRESS(index,label) gotoAddresses[index] = &&label
#  define GOTO_OPCODE() goto *gotoAddresses[*code]
#endif

template <typename T>
class vm_store
{
public:
	vm_store()
	{
		variables = new T[256];
	}

	~vm_store()
	{
		if (variables != nullptr)
			delete[] variables;
	}

	size_t startAddress;
	T* variables{ nullptr };
};

//template <int N>
//struct StaticAssignment {
//	inline static void assign(vm_char_t * data, char*& code)
//	{
//		data[N - 1] = *++code;
//		StaticAssignment<N - 1>::assign(data, code);
//	}
//};
//template <>
//struct StaticAssignment<1> {
//	inline static void assign(vm_char_t * data, char* & code)
//	{
//		data[0] = *++code;
//	}
//};

typedef void (two_arg_func)(vm_object*, vm_object*);
typedef void (one_arg_func)(vm_object*);

struct str_functions {
	two_arg_func add_str;
};

namespace
{
	const int STORE_SIZE = 1024;
	size_t storesCount;
    vm_store<Value>** stores{ nullptr };
    vm_store<Value>* currentStore{ nullptr };
    vm_store<Value> globalStore;
	std::unordered_map<string_type, VmMethod> nativeMethods;
	std::unordered_map<string_type, VmMethod>::iterator nativeMethodsEnd;
	std::unordered_map<string_type, size_t> methods;
	std::unordered_map<string_type, size_t>::iterator methodsEnd;

    Value* currentStack{ nullptr };
	size_t stackIndex;
	vm_system* system{ nullptr };

	void* gotoAddresses[128];

	void create(vm_system* pSystem)
	{
        currentStack = new Value[1024 * 512];
        currentStore = new vm_store<Value>;
        stores = new vm_store<Value>*[STORE_SIZE];

		for (size_t i = 0; i < STORE_SIZE; ++i)
            stores[i] = new vm_store<Value>;

		stores[0] = currentStore;
		storesCount = 0;
		stackIndex = 0;
		system = pSystem;
		nativeMethodsEnd = nativeMethods.end();
	}

	void destroy()
	{
		for (int i = 0; i < STORE_SIZE; ++i)
			delete stores[i];

		delete[] currentStack;
		delete[] stores;
	}


    inline static bool is_number(Value obj)
	{
        return IS_NUM(obj);
	}

	inline static bool is_number(char_type* chr)
	{
		char* end = 0;
		double val = strtod(chr, &end);
		return end != chr && val != HUGE_VAL;
	}

    inline static vm_object* get_as_string(Value val)
	{
        if (IS_OBJ(val))
        {
            vm_object* obj = AS_OBJ(val);
            switch (obj->Type)
            {
                case vm_object::vm_object_type::STR:
                    return new vm_object((char_type*)obj->Pointer);

                default:
                    return new vm_object(_T(""));
            }
        }

        return nullptr;
	}

	inline static vm_object* get_as_int(vm_object* obj)
	{

        return nullptr;
	}

	inline static vm_object* get_as_bool(vm_object* obj)
	{
        return nullptr;
	}

	inline static vm_object* get_as_double(vm_object* obj)
	{

        return nullptr;
	}

	inline static void add_str(vm_object* left, vm_object* right)
	{

	}

	inline static void add_double(vm_object* left, vm_object* right)
	{

	}

	inline static void add_int(vm_object* left, vm_object* right)
	{

	}

	inline static void add_bool(vm_object* left, vm_object* right)
	{

	}

	inline static void sub_double(vm_object* left, vm_object* right)
	{

	}

	inline static void sub_int(vm_object* left, vm_object* right)
	{

	}

	inline static void sub_bool(vm_object* left, vm_object* right)
	{

	}

	inline static void seq_str(vm_object* left, vm_object* right)
	{

	}

	inline static void mul_double(vm_object* left, vm_object* right)
	{

	}

	inline static void mul_int(vm_object* left, vm_object* right)
	{

	}

	inline static void mul_bool(vm_object* left, vm_object* right)
	{

	}

	inline static void div_double(vm_object* left, vm_object* right)
	{

	}

	inline static void div_int(vm_object* left, vm_object* right)
	{

	}

	inline static void div_bool(vm_object* left, vm_object* right)
	{

	}

	inline void eq_str(vm_object* left, vm_object* right)
	{

	}

	inline static void eq_double(vm_object* left, vm_object* right)
    {
	}

	inline static void eq_int(vm_object* left, vm_object* right)
	{

	}

	inline void eq_bool(vm_object* left, vm_object* right)
	{

	}

    static inline bool is_equal()
    {
        Value left = GET_VALUE(2);
        Value right = GET_VALUE(1);

        if (IS_BOOL(left) && IS_BOOL(right))
            return left == right;

        if (IS_NUM(left) && IS_NUM(right))
            return left == right;

        if ((IS_BOOL(left) && IS_NUM(right)) ||
            (IS_BOOL(right) && IS_NUM(left)))
        {
            if (IS_BOOL(left))
                return (double)AS_BOOL(left) == valueToNumber(right);

            return (double)AS_BOOL(right) == valueToNumber(left);
        }

		if (IS_STRING(right) || IS_STRING(left))
		{
			char* leftStr = nullptr;
			char* rightStr = nullptr;

			bool deleteLeftStr = false;
			bool deleteRightStr = false;

			if (IS_STRING(right))
				rightStr = (char_type*)AS_OBJ(right)->Pointer;
			else if (IS_NUM(right))
			{
				double val = valueToNumber(right);
				if (std::isinf(val) || std::isnan(val))
					rightStr = "";
				else
				{
					rightStr = new char_type[10];
					sprintf(rightStr, "%g", val);
				}

				deleteRightStr = true;
			}
			else if (IS_BOOL(right))
			{
				rightStr = new char_type[6];
				std::memcpy(rightStr, IS_FALSE(right) ? "false" : "true", 5);
				rightStr[5] = 0;
				deleteRightStr = true;
			}

			if (IS_STRING(left))
				leftStr = (char_type*)AS_OBJ(left)->Pointer;
			else if (IS_NUM(left))
			{
				double val = valueToNumber(left);
				if (std::isinf(val) || std::isnan(val))
					leftStr = "";
				else
				{
					leftStr = new char_type[10];
					sprintf(leftStr, "%g", val);
				}

				deleteLeftStr = true;
			}
			else if (IS_BOOL(left))
			{
				leftStr = new char_type[6];
				std::memcpy(leftStr, IS_FALSE(left) ? "false" : "true", 5);
				leftStr[5] = 0;
				deleteLeftStr = true;
			}

			bool status = std::strcmp(leftStr, rightStr) == 0;

			if (deleteLeftStr)
				delete leftStr;

			if (deleteRightStr)
				delete rightStr;

			return status;
		}

        return false;
    }

	void static execute(char_type* code, size_t len, size_t startIndex, bool firstInit)
	{
		char_type* startPoint = code;
		code += startIndex;

		if (firstInit)
			goto initSystem;

		PRINT_OPCODE();
		GOTO_OPCODE();

	opt_ADD:
		++code;
		PRINT_OPCODE();
        FUNC_BEGIN()
        {
            Value left = GET_VALUE(2);
            Value right = GET_VALUE(1);

            if (IS_NUM(left) && IS_NUM(right))
                GET_VALUE(2) = numberToValue(valueToNumber(left) + valueToNumber(right));
            else if (IS_BOOL(left) && IS_BOOL(right))
                GET_VALUE(2) = AS_BOOL(left) || AS_BOOL(right) ? TRUE_VAL : FALSE_VAL;
            else
                GET_VALUE(2) = NULL_VAL;

            STACK_DINC();
        }
        FUNC_END();
		GOTO_OPCODE();

	opt_SUB:
		++code;
		PRINT_OPCODE();
		FUNC_BEGIN()
        {
            Value left = GET_VALUE(2);
            Value right = GET_VALUE(1);

            if (IS_NUM(left) && IS_NUM(right))
                GET_VALUE(2) = numberToValue(valueToNumber(left) - valueToNumber(right));
            else if (IS_BOOL(left) && IS_BOOL(right))
                GET_VALUE(2) = AS_BOOL(left) && AS_BOOL(right) ? TRUE_VAL : FALSE_VAL;
            else
                GET_VALUE(2) = NULL_VAL;

            STACK_DINC();
        }
		FUNC_END();
		GOTO_OPCODE();

	opt_MUL:
		++code;
		PRINT_OPCODE();
		FUNC_BEGIN()
		{

			STACK_DINC();
		}
		FUNC_END();
		GOTO_OPCODE();

	opt_DIV:
		++code;
		PRINT_OPCODE();
		FUNC_BEGIN()
		{

			STACK_DINC();
		}
		FUNC_END();
		GOTO_OPCODE();

	opt_EQ:
		++code;
		PRINT_OPCODE();
		FUNC_BEGIN()
		{
			GET_VALUE(2) = is_equal() ? TRUE_VAL : FALSE_VAL;
			STACK_DINC();
		}
		FUNC_END();
		GOTO_OPCODE();

	opt_NOT_EQ:
		++code;
		PRINT_OPCODE();
		FUNC_BEGIN()
		{
			GET_VALUE(2) = is_equal() ? FALSE_VAL : TRUE_VAL;
			STACK_DINC();
		}
		FUNC_END();
		GOTO_OPCODE();

	opt_LT:
		++code;
		PRINT_OPCODE();
		{
			Value left = GET_VALUE(2);
			Value right = GET_VALUE(1);

			if (IS_NUM(left) && IS_NUM(right))
				GET_VALUE(2) = numberToValue(valueToNumber(left) < valueToNumber(right));
			else if (IS_BOOL(left) && IS_BOOL(right))
				GET_VALUE(2) = AS_BOOL(left) < AS_BOOL(right) ? TRUE_VAL : FALSE_VAL;
			else
				GET_VALUE(2) = NULL_VAL;

			STACK_DINC();
		}
		GOTO_OPCODE();

	opt_LTE:
		++code;
		PRINT_OPCODE();
		{
			Value left = GET_VALUE(2);
			Value right = GET_VALUE(1);

			if (IS_NUM(left) && IS_NUM(right))
				GET_VALUE(2) = numberToValue(valueToNumber(left) <= valueToNumber(right));
			else if (IS_BOOL(left) && IS_BOOL(right))
				GET_VALUE(2) = AS_BOOL(left) <= AS_BOOL(right) ? TRUE_VAL : FALSE_VAL;
			else
				GET_VALUE(2) = NULL_VAL;

			STACK_DINC();
		}
		GOTO_OPCODE();

	opt_GT:
		++code;
		PRINT_OPCODE();
		{
			Value left = GET_VALUE(2);
			Value right = GET_VALUE(1);

			if (IS_NUM(left) && IS_NUM(right))
				GET_VALUE(2) = numberToValue(valueToNumber(left) > valueToNumber(right));
			else if (IS_BOOL(left) && IS_BOOL(right))
				GET_VALUE(2) = AS_BOOL(left) > AS_BOOL(right) ? TRUE_VAL : FALSE_VAL;
			else
				GET_VALUE(2) = NULL_VAL;

			STACK_DINC();
		}
		GOTO_OPCODE();

	opt_GTE:
		++code;
		PRINT_OPCODE();
		{
			Value left = GET_VALUE(2);
			Value right = GET_VALUE(1);

			if (IS_NUM(left) && IS_NUM(right))
				GET_VALUE(2) = numberToValue(valueToNumber(left) >= valueToNumber(right));
			else if (IS_BOOL(left) && IS_BOOL(right))
				GET_VALUE(2) = AS_BOOL(left) >= AS_BOOL(right) ? TRUE_VAL : FALSE_VAL;
			else
				GET_VALUE(2) = NULL_VAL;

			STACK_DINC();
		}
		GOTO_OPCODE();

	opt_AND:
		++code;
		PRINT_OPCODE();
        FUNC_BEGIN()
        {
            Value left = GET_VALUE(2);
            Value right = GET_VALUE(1);

            GET_VALUE(2) = AS_BOOL(left) && AS_BOOL(right) ? TRUE_VAL : FALSE_VAL;
            STACK_DINC()
        }
        FUNC_END()
		GOTO_OPCODE();

	opt_OR:
		++code;
		PRINT_OPCODE();
        FUNC_BEGIN()
        {
            Value left = GET_VALUE(2);
            Value right = GET_VALUE(1);

            GET_VALUE(2) = AS_BOOL(left) || AS_BOOL(right) ? TRUE_VAL : FALSE_VAL;
            STACK_DINC()
        }
        FUNC_END()
		GOTO_OPCODE();

	opt_DUP:
		++code;
		PRINT_OPCODE();


		GOTO_OPCODE();

	opt_JMP:
		++code;
		{
			PRINT_OPCODE();
			FUNC_BEGIN()
            {
                vm_int_t integer;
                integer.Int = 0;

                ASSIGN_4(integer.Chars, code);

                code += integer.Int;
            }
			FUNC_END()
		}
		GOTO_OPCODE();

	opt_METHOD_DEF:
		++code;
		{
			PRINT_OPCODE();
			FUNC_BEGIN()
            {
                vm_int_t integer;
                integer.Int = 0;
                ASSIGN_4(integer.Chars, code);
                methods[integer.Chars] = startPoint - code;
            }
			FUNC_END()
		}
		GOTO_OPCODE();

	opt_JIF:
		++code;
		{
			PRINT_OPCODE();
			FUNC_BEGIN()
            {
                Value asdASD = POP_VALUE();
                bool a = AS_BOOL(asdASD);
                if (a)
					code += 4;
				else
				{
					vm_int_t integer;
					ASSIGN_4(integer.Chars, code);
					code += integer.Int;
				}
            }
			FUNC_END()
		}
		GOTO_OPCODE();

	opt_IF_EQ:
		++code;
		{
			PRINT_OPCODE();
			FUNC_BEGIN()
                if (is_equal())
					code += 4;
				else
				{
					vm_int_t integer;
					ASSIGN_4(integer.Chars, code);
					code += integer.Int;
				}

			stackIndex -= 2;
			FUNC_END()
		}
		GOTO_OPCODE();

	opt_JNIF:
		++code;
		{
			PRINT_OPCODE();
			FUNC_BEGIN()
                if (!is_equal())
				{
					vm_int_t integer;
					integer.Int = 0;
					ASSIGN_4(integer.Chars, code);
					code += integer.Int;
				}
				else
					code += 4;

			stackIndex -= 2;
			FUNC_END()
		}
		GOTO_OPCODE();

	opt_INC:
	opt_NEG:
    opt_DINC:
        GOTO_OPCODE();

	opt_LOAD:
		++code;
		{
			PRINT_OPCODE();
			int data = *++code;
            LOAD_AND_PUSH(data);
		}
		GOTO_OPCODE();

	LOAD_PRE(0);
    LOAD_PRE(1);
    LOAD_PRE(2);
    LOAD_PRE(3);
    LOAD_PRE(4);
    LOAD_PRE(5);
    LOAD_PRE(6);
    LOAD_PRE(7);
    LOAD_PRE(8);
    LOAD_PRE(9);
    LOAD_PRE(10);
    LOAD_PRE(11);
    LOAD_PRE(12);
    LOAD_PRE(13);
    LOAD_PRE(14);
    LOAD_PRE(15);
    LOAD_PRE(16);

	opt_STORE:
		++code;
		PRINT_OPCODE();
		STORE(*++code, POP());
		GOTO_OPCODE();

    STORE_PRE(0);
    STORE_PRE(1);
    STORE_PRE(2);
    STORE_PRE(3);
    STORE_PRE(4);
    STORE_PRE(5);
    STORE_PRE(6);
    STORE_PRE(7);
    STORE_PRE(8);
    STORE_PRE(9);
    STORE_PRE(10);
    STORE_PRE(11);
    STORE_PRE(12);
    STORE_PRE(13);
    STORE_PRE(14);
    STORE_PRE(15);
    STORE_PRE(16);

	opt_GLOAD:
		++code;
		PRINT_OPCODE();
		PUSH_WITH_ASSIGN(GLOAD(*++code));
		GOTO_OPCODE();

    GLOAD_PRE(0);
    GLOAD_PRE(1);
    GLOAD_PRE(2);
    GLOAD_PRE(3);
    GLOAD_PRE(4);
    GLOAD_PRE(5);
    GLOAD_PRE(6);
    GLOAD_PRE(7);
    GLOAD_PRE(8);
    GLOAD_PRE(9);
    GLOAD_PRE(10);
    GLOAD_PRE(11);
    GLOAD_PRE(12);
    GLOAD_PRE(13);
    GLOAD_PRE(14);
    GLOAD_PRE(15);
    GLOAD_PRE(16);

	opt_GSTORE:
		++code;
		PRINT_OPCODE();
		GSTORE(*++code, POP());
		GOTO_OPCODE();

    GSTORE_PRE(0);
    GSTORE_PRE(1);
    GSTORE_PRE(2);
    GSTORE_PRE(3);
    GSTORE_PRE(4);
    GSTORE_PRE(5);
    GSTORE_PRE(6);
    GSTORE_PRE(7);
    GSTORE_PRE(8);
    GSTORE_PRE(9);
    GSTORE_PRE(10);
    GSTORE_PRE(11);
    GSTORE_PRE(12);
    GSTORE_PRE(13);
    GSTORE_PRE(14);
    GSTORE_PRE(15);
    GSTORE_PRE(16);

	opt_CALL:
		++code;
		{
			PRINT_OPCODE();
			FUNC_BEGIN()
            {
                currentStore = stores[++storesCount];
                vm_int_t integer;
                integer.Int = 0;
                ASSIGN_4(integer.Chars, code);
                currentStore->startAddress = (code - startPoint);
                code += integer.Int;
            }
			FUNC_END()
		}
		GOTO_OPCODE();

	opt_RETURN:
		++code;
		{
			PRINT_OPCODE();
			FUNC_BEGIN()
            {
                code = startPoint + currentStore->startAddress;
                currentStore = stores[--storesCount];
            }
			FUNC_END()
		}
		GOTO_OPCODE();

	opt_POP:
		++code;
		PRINT_OPCODE();
		POP();
		GOTO_OPCODE();

    opt_CONST_INT:
		++code; {
			PRINT_OPCODE();
			FUNC_BEGIN()
            {
                vm_int_t integer;
                integer.Int = 0;
                ASSIGN_4(integer.Chars, code);
                PUSH_WITH_ASSIGN_INT(integer.Int);
            }
            FUNC_END()
				// console_out << _T("PUSH : ") << Operations::Peek<int>(currentStack, stackIndex) << '\n';
		}
		GOTO_OPCODE();

    opt_CONST_INT_0:
		++code;
		PRINT_OPCODE();
        PUSH_WITH_ASSIGN_INT(0);
		GOTO_OPCODE();

    opt_CONST_INT_1:
		++code;
		PRINT_OPCODE();
        PUSH_WITH_ASSIGN_INT(1);
		GOTO_OPCODE();

    opt_CONST_DOUBLE:
        ++code; {
            PRINT_OPCODE();
            vm_double_t d;
            d.Double = 0.0;
            ASSIGN_8(d.Chars, code);
            PUSH_WITH_ASSIGN_INT(d.Double);
        }
        GOTO_OPCODE();

    opt_CONST_DOUBLE_0:
        ++code; {
            PRINT_OPCODE();
            vm_double_t d;
            d.Double = 0.0;
            ASSIGN_8(d.Chars, code);
            PUSH_WITH_ASSIGN_INT(d.Double);
        }
        GOTO_OPCODE();

    opt_CONST_DOUBLE_1:
        ++code; {
            PRINT_OPCODE();
            vm_double_t d;
            d.Double = 1.0;
            PUSH_WITH_ASSIGN_INT(d.Double);
        }
        GOTO_OPCODE();


    opt_CONST_BOOL_TRUE:
        ++code;
        PRINT_OPCODE();
        PUSH_WITH_ASSIGN(TRUE_VAL);
        GOTO_OPCODE();

    opt_CONST_BOOL_FALSE:
        ++code;
        PRINT_OPCODE();
		PUSH_WITH_ASSIGN(FALSE_VAL);
        GOTO_OPCODE();

	opt_CALL_NATIVE:
		++code; {
			PRINT_OPCODE();
			vm_int_t integer;
			integer.Int = 0;
			ASSIGN_4(integer.Chars, code);

			char_type * chars = new char_type[integer.Int + 1];
			for (int i = 0; i < integer.Int; ++i)
			{
				chars[i] = *code;
				++code;
			}

			chars[integer.Int] = '\0';
			if (nativeMethods.find(chars) != nativeMethodsEnd)
			{
				vm_object* result = nativeMethods[chars](system);
				if (result != nullptr)
				{
                    /*switch (result->Type)
					{
					case vm_object::vm_object_type::BOOL:
						PUSH_WITH_INIT(result->Bool);
						break;

					case vm_object::vm_object_type::DOUBLE:
						PUSH_WITH_INIT(result->Double);
						GOTO_OPCODE();

					case vm_object::vm_object_type::INT:
						PUSH_WITH_INIT(result->Int);
						break;

					case vm_object::vm_object_type::STR:
						PUSH_WITH_INIT(static_cast<char_type*>(result->Pointer));
						break;
                    }*/
				}
			}
			else
			{
				console_out << _T("ERROR : Method '") << chars << _T("' Not Found\n");
				PUSH_WITH_EMPTY();
				goto opt_HALT;
			}
		}
		GOTO_OPCODE();

    opt_CONST_STR:
		++code; {
			PRINT_OPCODE();
			vm_int_t integer;
			integer.Int = 0;
			ASSIGN_4(integer.Chars, code);

			char_type * chars = new char_type[integer.Int + 1];
			for (int i = 0; i < integer.Int; ++i)
			{
				chars[i] = *code;
				++code;
			}

			chars[integer.Int] = '\0';
			PUSH_WITH_INIT(chars);
		}
		GOTO_OPCODE();

	opt_PRINT:
		++code;
		PRINT_OPCODE();
		// console_out << Operations::PopAs<int>(currentStack, stackIndex);
		GOTO_OPCODE();

	opt_HALT:
		++code;
		PRINT_OPCODE();
		return;

	initSystem:
		void** testCodes = gotoAddresses; // this dummy codes used by vc++ 
		STORE_ADDRESS(0 /*OPT_HALT*/, opt_HALT);
		STORE_ADDRESS(1 /*OPT_ADD*/, opt_ADD);
		STORE_ADDRESS(2 /*OPT_SUB*/, opt_SUB);
		STORE_ADDRESS(3 /*OPT_MUL*/, opt_MUL);
		STORE_ADDRESS(4 /*OPT_DIV*/, opt_DIV);
		STORE_ADDRESS(5 /*OPT_EQ*/, opt_EQ);
		STORE_ADDRESS(6 /*OPT_LT*/, opt_LT);
		STORE_ADDRESS(7 /*OPT_LTE*/, opt_LTE);
		STORE_ADDRESS(8 /*OPT_GT*/, opt_GT);
		STORE_ADDRESS(9 /*OPT_GTE*/, opt_GTE);
		STORE_ADDRESS(10 /*OPT_AND*/, opt_AND);
		STORE_ADDRESS(11 /*OPT_OR*/, opt_OR);
		STORE_ADDRESS(12 /*OPT_DUP*/, opt_DUP);
		STORE_ADDRESS(13 /*OPT_POP*/, opt_POP);
		STORE_ADDRESS(14 /*OPT_CONST_STR*/, opt_CONST_STR);
		STORE_ADDRESS(15 /*OPT_CONST_INT*/, opt_CONST_INT);
		STORE_ADDRESS(16 /*OPT_CONST_INT_0*/, opt_CONST_INT_0);
		STORE_ADDRESS(17 /*OPT_CONST_INT_1*/, opt_CONST_INT_1);
		STORE_ADDRESS(18 /*OPT_CONST_BOOL_TRUE*/, opt_CONST_BOOL_TRUE);
		STORE_ADDRESS(19 /*OPT_CONST_BOOL_FALSE*/, opt_CONST_BOOL_FALSE);
		STORE_ADDRESS(20 /*OPT_CONST_DOUBLE*/, opt_CONST_DOUBLE);
		STORE_ADDRESS(21 /*OPT_CONST_DOUBLE_0*/, opt_CONST_DOUBLE_0);
		STORE_ADDRESS(22 /*OPT_CONST_DOUBLE_1*/, opt_CONST_DOUBLE_1);
//		STORE_ADDRESS(23 /*OPT_DELETE*/, opt_DELETE);
		STORE_ADDRESS(24 /*OPT_JMP*/, opt_JMP);
		STORE_ADDRESS(25 /*OPT_IF_EQ*/, opt_IF_EQ);
		STORE_ADDRESS(26 /*OPT_JIF*/, opt_JIF);
		STORE_ADDRESS(27 /*OPT_JNIF*/, opt_JNIF);
		STORE_ADDRESS(28 /*OPT_INC*/, opt_INC);
		STORE_ADDRESS(29 /*OPT_DINC*/, opt_DINC);
		STORE_ADDRESS(30 /*OPT_LOAD*/, opt_LOAD);
		STORE_ADDRESS(31 /*OPT_LOAD_0*/, opt_LOAD_0);
		STORE_ADDRESS(32 /*OPT_LOAD_1*/, opt_LOAD_1);
		STORE_ADDRESS(33 /*OPT_LOAD_2*/, opt_LOAD_2);
		STORE_ADDRESS(34 /*OPT_LOAD_3*/, opt_LOAD_3);
		STORE_ADDRESS(35 /*OPT_LOAD_4*/, opt_LOAD_4);
		STORE_ADDRESS(36 /*OPT_LOAD_5*/, opt_LOAD_5);
		STORE_ADDRESS(37 /*OPT_LOAD_6*/, opt_LOAD_6);
		STORE_ADDRESS(38 /*OPT_LOAD_7*/, opt_LOAD_7);
		STORE_ADDRESS(39 /*OPT_LOAD_8*/, opt_LOAD_8);
		STORE_ADDRESS(40 /*OPT_LOAD_9*/, opt_LOAD_9);
		STORE_ADDRESS(41 /*OPT_LOAD_10*/, opt_LOAD_10);
		STORE_ADDRESS(42 /*OPT_LOAD_11*/, opt_LOAD_11);
		STORE_ADDRESS(43 /*OPT_LOAD_12*/, opt_LOAD_12);
		STORE_ADDRESS(44 /*OPT_LOAD_13*/, opt_LOAD_13);
		STORE_ADDRESS(45 /*OPT_LOAD_14*/, opt_LOAD_14);
		STORE_ADDRESS(46 /*OPT_LOAD_15*/, opt_LOAD_15);
		STORE_ADDRESS(47 /*OPT_LOAD_16*/, opt_LOAD_16);
		STORE_ADDRESS(48 /*OPT_STORE*/, opt_STORE);
		STORE_ADDRESS(49 /*OPT_STORE_0*/, opt_STORE_0);
		STORE_ADDRESS(50 /*OPT_STORE_1*/, opt_STORE_1);
		STORE_ADDRESS(51 /*OPT_STORE_2*/, opt_STORE_2);
		STORE_ADDRESS(52 /*OPT_STORE_3*/, opt_STORE_3);
		STORE_ADDRESS(53 /*OPT_STORE_4*/, opt_STORE_4);
		STORE_ADDRESS(54 /*OPT_STORE_5*/, opt_STORE_5);
		STORE_ADDRESS(55 /*OPT_STORE_6*/, opt_STORE_6);
		STORE_ADDRESS(56 /*OPT_STORE_7*/, opt_STORE_7);
		STORE_ADDRESS(57 /*OPT_STORE_8*/, opt_STORE_8);
		STORE_ADDRESS(58 /*OPT_STORE_9*/, opt_STORE_9);
		STORE_ADDRESS(59 /*OPT_STORE_10*/, opt_STORE_10);
		STORE_ADDRESS(60 /*OPT_STORE_11*/, opt_STORE_11);
		STORE_ADDRESS(61 /*OPT_STORE_12*/, opt_STORE_12);
		STORE_ADDRESS(62 /*OPT_STORE_13*/, opt_STORE_13);
		STORE_ADDRESS(63 /*OPT_STORE_14*/, opt_STORE_14);
		STORE_ADDRESS(64 /*OPT_STORE_15*/, opt_STORE_15);
		STORE_ADDRESS(65 /*OPT_STORE_16*/, opt_STORE_16);
		STORE_ADDRESS(66 /*OPT_GLOAD*/, opt_GLOAD);
		STORE_ADDRESS(67 /*OPT_GLOAD_0*/, opt_GLOAD_0);
		STORE_ADDRESS(68 /*OPT_GLOAD_1*/, opt_GLOAD_1);
		STORE_ADDRESS(69 /*OPT_GLOAD_2*/, opt_GLOAD_2);
		STORE_ADDRESS(70 /*OPT_GLOAD_3*/, opt_GLOAD_3);
		STORE_ADDRESS(71 /*OPT_GLOAD_4*/, opt_GLOAD_4);
		STORE_ADDRESS(72 /*OPT_GLOAD_5*/, opt_GLOAD_5);
		STORE_ADDRESS(73 /*OPT_GLOAD_6*/, opt_GLOAD_6);
		STORE_ADDRESS(74 /*OPT_GLOAD_7*/, opt_GLOAD_7);
		STORE_ADDRESS(75 /*OPT_GLOAD_8*/, opt_GLOAD_8);
		STORE_ADDRESS(76 /*OPT_GLOAD_9*/, opt_GLOAD_9);
		STORE_ADDRESS(77 /*OPT_GLOAD_10*/, opt_GLOAD_10);
		STORE_ADDRESS(78 /*OPT_GLOAD_11*/, opt_GLOAD_11);
		STORE_ADDRESS(79 /*OPT_GLOAD_12*/, opt_GLOAD_12);
		STORE_ADDRESS(80 /*OPT_GLOAD_13*/, opt_GLOAD_13);
		STORE_ADDRESS(81 /*OPT_GLOAD_14*/, opt_GLOAD_14);
		STORE_ADDRESS(82 /*OPT_GLOAD_15*/, opt_GLOAD_15);
		STORE_ADDRESS(83 /*OPT_GLOAD_16*/, opt_GLOAD_16);
		STORE_ADDRESS(84 /*OPT_GSTORE*/, opt_GSTORE);
		STORE_ADDRESS(85 /*OPT_GSTORE_0*/, opt_GSTORE_0);
		STORE_ADDRESS(86 /*OPT_GSTORE_1*/, opt_GSTORE_1);
		STORE_ADDRESS(87 /*OPT_GSTORE_2*/, opt_GSTORE_2);
		STORE_ADDRESS(88 /*OPT_GSTORE_3*/, opt_GSTORE_3);
		STORE_ADDRESS(89 /*OPT_GSTORE_4*/, opt_GSTORE_4);
		STORE_ADDRESS(90 /*OPT_GSTORE_5*/, opt_GSTORE_5);
		STORE_ADDRESS(91 /*OPT_GSTORE_6*/, opt_GSTORE_6);
		STORE_ADDRESS(92 /*OPT_GSTORE_7*/, opt_GSTORE_7);
		STORE_ADDRESS(93 /*OPT_GSTORE_8*/, opt_GSTORE_8);
		STORE_ADDRESS(94 /*OPT_GSTORE_9*/, opt_GSTORE_9);
		STORE_ADDRESS(95 /*OPT_GSTORE_10*/, opt_GSTORE_10);
		STORE_ADDRESS(96 /*OPT_GSTORE_11*/, opt_GSTORE_11);
		STORE_ADDRESS(97 /*OPT_GSTORE_12*/, opt_GSTORE_12);
		STORE_ADDRESS(98 /*OPT_GSTORE_13*/, opt_GSTORE_13);
		STORE_ADDRESS(99 /*OPT_GSTORE_14*/, opt_GSTORE_14);
		STORE_ADDRESS(100 /*OPT_GSTORE_15*/, opt_GSTORE_15);
		STORE_ADDRESS(101 /*OPT_GSTORE_16*/, opt_GSTORE_16);
		STORE_ADDRESS(102 /*OPT_CALL*/, opt_CALL);
		STORE_ADDRESS(103 /*OPT_RETURN*/, opt_RETURN);
//		STORE_ADDRESS(104 /*OPT_PUSH*/, opt_PUSH);
		STORE_ADDRESS(105 /*OPT_PRINT*/, opt_PRINT);
		STORE_ADDRESS(106 /*OPT_NEG*/, opt_NEG);
		STORE_ADDRESS(107 /*OPT_CALL_NATIVE*/, opt_CALL_NATIVE);
		STORE_ADDRESS(108 /*OPT_METHOD_DEF*/, opt_METHOD_DEF);
//		STORE_ADDRESS(109 /*OPT_INITARRAY*/, opt_INITARRAY);
//		STORE_ADDRESS(110 /*OPT_INITDICT*/, opt_INITDICT);
		STORE_ADDRESS(111 /*OPT_NOT_EQ*/, opt_NOT_EQ);

	}

	static void dumpOpcode(char_type* code, size_t len)
	{
        size_t index = 0;
		while (index < len) {
			console_out << _T(">>> ") << index++ << _T(". ");
			console_out << vm_instToString((vm_inst)*code);

			switch ((vm_inst)*code)
			{
			case vm_inst::OPT_LOAD:
			case vm_inst::OPT_STORE:
			case vm_inst::OPT_GLOAD:
			case vm_inst::OPT_GSTORE:
			case vm_inst::OPT_CALL:
			{
				++code;
				vm_int_t integer;
				integer.Int = 0;
				ASSIGN_4(integer.Chars, code);
				console_out << _T(" ") << integer.Int;
				index += 4;
				--code;
			}
			break;

			case vm_inst::OPT_JMP:
			case vm_inst::OPT_JIF:
			case vm_inst::OPT_IF_EQ:
			case vm_inst::OPT_JNIF:
            case vm_inst::OPT_CONST_INT:
			{
			    ++code;
				vm_int_t integer;
				integer.Int = 0;
				ASSIGN_4(integer.Chars, code);
				console_out << _T(" ") << integer.Int;
				index += 4;
				--code;
			}
			break;

            case vm_inst::OPT_CONST_DOUBLE:
			{
				++code;
				vm_double_t d;
				d.Double = 0;
				ASSIGN_8(d.Chars, code);

				console_out << _T(" ") << d.Double;
				index += 8;
				--code;
			}
			break;

                case vm_inst::OPT_CONST_BOOL_TRUE:
                case vm_inst::OPT_CONST_BOOL_FALSE:
				// console_out << _T(" ") << (bool)*++code;
				++index;
				break;

            case vm_inst::OPT_CALL_NATIVE:
			{
				vm_int_t integer;
				integer.Int = 0;
				ASSIGN_1(integer.Chars, code);
				index += 1;
				index += integer.Int;

				char_type * chars = new char_type[integer.Int + 1];
				for (int i = integer.Int - 1; i >= 0; --i)
					chars[i] = *++code;

				chars[integer.Int] = '\0';

				console_out << _T(" \"") << chars << _T("\"");
			}
			break;

			case vm_inst::OPT_METHOD_DEF:
			{
				++code;
				vm_int_t integer;
				integer.Int = 0;
				ASSIGN_4(integer.Chars, code);
				index += 4;
				index += integer.Int;

				char_type * chars = new char_type[integer.Int + 1];
				for (int i = 0; i < integer.Int; ++i)
				{
					chars[i] = *code;
					++code;
				}

				chars[integer.Int] = '\0';

				console_out << _T(" \"") << chars << _T("\"");
				--code;
			}
			break;

            case vm_inst::OPT_CONST_STR:
			{
				vm_int_t integer;
				integer.Int = 0;
				ASSIGN_4(integer.Chars, code);
				index += 4;
				index += integer.Int;

				char_type * chars = new char_type[integer.Int + 1];
				for (int i = integer.Int - 1; i >= 0; --i)
					chars[i] = *++code;

				chars[integer.Int] = '\0';

				console_out << _T(" \"") << chars << _T("\"");
			}
			break;
			}

            ++code;
			console_out << '\n';
		}

		console_out << '\n';
	}

	static void dumpStack()
	{
		int index = stackIndex;
        /*while (index > 0) {
			console_out << _T(">>> ") << index << _T(". ");
			vm_object item = currentStack[index];
			switch (item.Type)
			{
			case vm_object::vm_object_type::INT:
				console_out << _T(" INT: ");
				console_out << item.Int;
				break;

			case vm_object::vm_object_type::DOUBLE:
				console_out << _T(" DOUBLE: ");
				console_out << item.Double;
				break;

			case vm_object::vm_object_type::BOOL:
				console_out << _T(" BOOL: ");
				console_out << item.Bool;
				break;

			case vm_object::vm_object_type::STR:
				console_out << _T(" STR: ");
				console_out << string_type(*static_cast<string_type*>(item.Pointer));
				break;

			default:
				console_out << _T(" EMPTY: ");
				break;
			}

			console_out << '\n';

			--index;
		}
*/
		console_out << '\n';
	}

	static void dump(char_type* code, size_t len)
	{
		size_t index = 0;
		while (index < len) {
			console_out << _T(">>> ") << index++ << _T(". ");
			console_out << int(*code);
			console_out << '\n';
			++code;
		}

		console_out << '\n';
	}
}

vm_system::vm_system()
{
	create(this);
	::execute(nullptr, 0, 0, true);
}

vm_system::~vm_system()
{
	delete impl;
}

void vm_system::execute(char_type* code, size_t len, size_t startIndex)
{
	::execute(code, len, startIndex, false);
}

void vm_system::dump(char_type* code, size_t len)
{
	::dump(code, len);
}

void vm_system::dumpOpcode(char_type* code, size_t len)
{
	::dumpOpcode(code, len);
}

void vm_system::dumpStack()
{
	::dumpStack();
}

size_t vm_system::getUInt()
{

    return numberToValue(currentStack[::stackIndex - 1]);
}

Value vm_system::getObject()
{
    if (::stackIndex > 0)
    {
        //console_out << ::stackIndex << '\n';
        return ::currentStack[--::stackIndex];
    }

    return NULL_VAL;
}

void vm_system::addMethod(string_type const & name, VmMethod method)
{
	::nativeMethods[name] = method;
	::nativeMethodsEnd = ::nativeMethods.end();
}
