#pragma once
#include "Interface.h"
#include "Object.h"
#include <deque>

class ArgumentPack
{
public:
	template <typename... Args>
	ArgumentPack(Args&&... args)
		: m_Args{ Object::Box(std::forward<Args>(args))... }
	{
	}
	~ArgumentPack();

	natRefPointer<Object> Extract();
	natRefPointer<Object> Get(size_t n) const;
	natRefPointer<IType> GetType(size_t n) const;
	size_t Size() const;

private:
	std::deque<natRefPointer<Object>> m_Args;
};

namespace rdetail_
{
	struct forward_call_t
	{
		constexpr forward_call_t() noexcept = default;
	};

	constexpr forward_call_t forward_call;

	template <typename T, typename RetVoidTest = void>
	struct InvokeNonMemberHelper
	{
		static natRefPointer<Object> Invoke(typename T::MethodType method, ArgumentPack const& pack)
		{
			return Object::Box(T::InvokeWithArgPack(method, pack));
		}
	};

	template <typename T>
	struct InvokeNonMemberHelper<T, std::void_t<std::enable_if_t<std::is_void<typename T::ReturnType>::value>>>
	{
		static natRefPointer<Object> Invoke(typename T::MethodType method, ArgumentPack const& pack)
		{
			T::InvokeWithArgPack(method, pack);
			return Object::Box();
		}
	};

	template <typename T, typename RetVoidTest = void>
	struct InvokeMemberHelper
	{
		static natRefPointer<Object> Invoke(natRefPointer<Object> object, typename T::MethodType method, ArgumentPack const& pack)
		{
			return Object::Box(T::InvokeWithArgPack(&object->Unbox<typename T::ClassType>(), method, pack));
		}
	};

	template <typename T>
	struct InvokeMemberHelper<T, std::void_t<std::enable_if_t<std::is_void<typename T::ReturnType>::value>>>
	{
		static natRefPointer<Object> Invoke(natRefPointer<Object> object, typename T::MethodType method, ArgumentPack const& pack)
		{
			T::InvokeWithArgPack(&object->Unbox<typename T::ClassType>(), method, pack);
			return Object::Box();
		}
	};
}

template <typename Func>
struct MethodHelper;

template <typename Ret, typename... Args>
struct MethodHelper<Ret(*)(Args...)>
{
	typedef Ret(*MethodType)(Args...);
	typedef Ret ReturnType;

	static decltype(auto) InvokeWithArgs(MethodType method, Args... args)
	{
		return method(static_cast<Args>(args)...);
	}

	static decltype(auto) InvokeWithArgPack(MethodType method, ArgumentPack const& pack)
	{
		return InvokeWithArgPackHelper(method, pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static natRefPointer<Object> Invoke(MethodType method, ArgumentPack const& pack)
	{
		return rdetail_::InvokeNonMemberHelper<MethodHelper>::Invoke(method, pack);
	}

	static bool CompatWith(ArgumentPack const& pack)
	{
		return CompatWithImpl(pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static std::vector<natRefPointer<IType>> GetType()
	{
		return{ Reflection::GetInstance().GetType<Ret>(), Reflection::GetInstance().GetType<Args>()... };
	}

private:
	template <size_t... i>
	static decltype(auto) InvokeWithArgPackHelper(MethodType method, ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return InvokeWithArgs(method, static_cast<Args>(pack.Get(i)->Unbox<std::remove_cv_t<std::remove_reference_t<Args>>>())...);
	}

	template <size_t... i>
	static bool CompatWithImpl(ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return pack.Size() == sizeof...(i) && std::make_tuple((pack.GetType(i)->GetTypeIndex() == typeid(boxed_type_t<Args>))...) == std::make_tuple(std::bool_constant<i >= 0>::value...);
	}
};

template <typename Ret, typename... Args>
struct MethodHelper<Ret(*)(rdetail_::forward_call_t, std::tuple<Args...>&&)>
{
	typedef Ret(*MethodType)(rdetail_::forward_call_t, std::tuple<Args...>&&);
	typedef Ret ReturnType;

	static decltype(auto) InvokeWithArgs(MethodType method, Args... args)
	{
		return method(rdetail_::forward_call, std::forward_as_tuple(static_cast<Args>(args)...));
	}

	static decltype(auto) InvokeWithArgPack(MethodType method, ArgumentPack const& pack)
	{
		return InvokeWithArgPackHelper(method, pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static natRefPointer<Object> Invoke(MethodType method, ArgumentPack const& pack)
	{
		return rdetail_::InvokeNonMemberHelper<MethodHelper>::Invoke(method, pack);
	}

	static bool CompatWith(ArgumentPack const& pack)
	{
		return CompatWithImpl(pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static std::vector<natRefPointer<IType>> GetType()
	{
		return{ Reflection::GetInstance().GetType<Ret>(), Reflection::GetInstance().GetType<Args>()... };
	}

private:
	template <size_t... i>
	static decltype(auto) InvokeWithArgPackHelper(MethodType method, ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return InvokeWithArgs(method, static_cast<Args>(pack.Get(i)->Unbox<std::remove_cv_t<std::remove_reference_t<Args>>>())...);
	}

	template <size_t... i>
	static bool CompatWithImpl(ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return pack.Size() == sizeof...(i) && std::make_tuple((pack.GetType(i)->GetTypeIndex() == typeid(boxed_type_t<Args>))...) == std::make_tuple(std::bool_constant<i >= 0>::value...);
	}
};

template <typename Ret, typename Class, typename... Args>
struct MethodHelper<Ret(Class::*)(Args...)>
{
	typedef Ret(Class::*MethodType)(Args...);
	typedef Ret ReturnType;
	typedef Class ClassType;

	static decltype(auto) InvokeWithArgs(Class* object, MethodType method, Args... args)
	{
		return (object->*method)(static_cast<Args>(args)...);
	}

	static decltype(auto) InvokeWithArgPack(Class* object, MethodType method, ArgumentPack const& pack)
	{
		return InvokeWithArgPackHelper(object, method, pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static natRefPointer<Object> Invoke(natRefPointer<Object> object, MethodType method, ArgumentPack const& pack)
	{
		return rdetail_::InvokeMemberHelper<MethodHelper>::Invoke(object, method, pack);
	}

	static bool CompatWith(natRefPointer<Object> object, ArgumentPack const& pack)
	{
		return object->GetType()->GetTypeIndex() == typeid(Class) && CompatWithImpl(pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static std::vector<natRefPointer<IType>> GetType()
	{
		return{ Reflection::GetInstance().GetType<Ret>(), Reflection::GetInstance().GetType<Class>(), Reflection::GetInstance().GetType<Args>()... };
	}

private:
	template <size_t... i>
	static decltype(auto) InvokeWithArgPackHelper(Class* object, MethodType method, ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return InvokeWithArgs(object, method, static_cast<Args>(pack.Get(i)->Unbox<std::remove_cv_t<std::remove_reference_t<Args>>>())...);
	}

	template <size_t... i>
	static bool CompatWithImpl(ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return pack.Size() == sizeof...(i) && std::make_tuple((pack.GetType(i)->GetTypeIndex() == typeid(boxed_type_t<Args>))...) == std::make_tuple(std::bool_constant<i >= 0>::value...);
	}
};

template <typename Ret, typename Class, typename... Args>
struct MethodHelper<Ret(Class::*)(rdetail_::forward_call_t, std::tuple<Args...>&&)>
{
	typedef Ret(Class::*MethodType)(rdetail_::forward_call_t, std::tuple<Args...>&&);
	typedef Ret ReturnType;
	typedef Class ClassType;

	static decltype(auto) InvokeWithArgs(Class* object, MethodType method, Args... args)
	{
		return (object->*method)(rdetail_::forward_call, std::forward_as_tuple(static_cast<Args>(args)...));
	}

	static decltype(auto) InvokeWithArgPack(Class* object, MethodType method, ArgumentPack const& pack)
	{
		return InvokeWithArgPackHelper(object, method, pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static natRefPointer<Object> Invoke(natRefPointer<Object> object, MethodType method, ArgumentPack const& pack)
	{
		return rdetail_::InvokeMemberHelper<MethodHelper>::Invoke(object, method, pack);
	}

	static bool CompatWith(natRefPointer<Object> object, ArgumentPack const& pack)
	{
		return object->GetType()->GetTypeIndex() == typeid(Class) && CompatWithImpl(pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static std::vector<natRefPointer<IType>> GetType()
	{
		return{ Reflection::GetInstance().GetType<Ret>(), Reflection::GetInstance().GetType<Class>(), Reflection::GetInstance().GetType<Args>()... };
	}

private:
	template <size_t... i>
	static decltype(auto) InvokeWithArgPackHelper(Class* object, MethodType method, ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return InvokeWithArgs(object, method, static_cast<Args>(pack.Get(i)->Unbox<std::remove_cv_t<std::remove_reference_t<Args>>>())...);
	}

	template <size_t... i>
	static bool CompatWithImpl(ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return pack.Size() == sizeof...(i) && std::make_tuple((pack.GetType(i)->GetTypeIndex() == typeid(boxed_type_t<Args>))...) == std::make_tuple(std::bool_constant<i >= 0>::value...);
	}
};

template <typename Ret, typename Class, typename... Args>
struct MethodHelper<Ret(Class::*)(Args...) const>
{
	typedef Ret(Class::*MethodType)(Args...) const;
	typedef Ret ReturnType;
	typedef Class ClassType;

	static decltype(auto) InvokeWithArgs(const Class* object, MethodType method, Args... args)
	{
		return (object->*method)(static_cast<Args>(args)...);
	}

	static decltype(auto) InvokeWithArgPack(const Class* object, MethodType method, ArgumentPack const& pack)
	{
		return InvokeWithArgPackHelper(object, method, pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static natRefPointer<Object> Invoke(natRefPointer<Object> object, MethodType method, ArgumentPack const& pack)
	{
		return rdetail_::InvokeMemberHelper<MethodHelper>::Invoke(object, method, pack);
	}

	static bool CompatWith(natRefPointer<Object> object, ArgumentPack const& pack)
	{
		return object->GetType()->GetTypeIndex() == typeid(Class) && CompatWithImpl(pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static std::vector<natRefPointer<IType>> GetType()
	{
		return{ Reflection::GetInstance().GetType<Ret>(), Reflection::GetInstance().GetType<Class>(), Reflection::GetInstance().GetType<Args>()... };
	}

private:
	template <size_t... i>
	static decltype(auto) InvokeWithArgPackHelper(const Class* object, MethodType method, ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return InvokeWithArgs(object, method, static_cast<Args>(pack.Get(i)->Unbox<std::remove_cv_t<std::remove_reference_t<Args>>>())...);
	}

	template <size_t... i>
	static bool CompatWithImpl(ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return pack.Size() == sizeof...(i) && std::make_tuple((pack.GetType(i)->GetTypeIndex() == typeid(boxed_type_t<Args>))...) == std::make_tuple(std::bool_constant<i >= 0>::value...);
	}
};

template <typename Ret, typename Class, typename... Args>
struct MethodHelper<Ret(Class::*)(rdetail_::forward_call_t, std::tuple<Args...>&&) const>
{
	typedef Ret(Class::*MethodType)(rdetail_::forward_call_t, std::tuple<Args...>&&) const;
	typedef Ret ReturnType;
	typedef Class ClassType;

	static decltype(auto) InvokeWithArgs(const Class* object, MethodType method, Args... args)
	{
		return (object->*method)(rdetail_::forward_call, std::forward_as_tuple(static_cast<Args>(args)...));
	}

	static decltype(auto) InvokeWithArgPack(const Class* object, MethodType method, ArgumentPack const& pack)
	{
		return InvokeWithArgPackHelper(object, method, pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static natRefPointer<Object> Invoke(natRefPointer<Object> object, MethodType method, ArgumentPack const& pack)
	{
		return rdetail_::InvokeMemberHelper<MethodHelper>::Invoke(object, method, pack);
	}

	static bool CompatWith(natRefPointer<Object> object, ArgumentPack const& pack)
	{
		return object->GetType()->GetTypeIndex() == typeid(Class) && CompatWithImpl(pack, typename std::make_index_sequence<sizeof...(Args)>::type{});
	}

	static std::vector<natRefPointer<IType>> GetType()
	{
		return{ Reflection::GetInstance().GetType<Ret>(), Reflection::GetInstance().GetType<Class>(), Reflection::GetInstance().GetType<Args>()... };
	}

private:
	template <size_t... i>
	static decltype(auto) InvokeWithArgPackHelper(const Class* object, MethodType method, ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return InvokeWithArgs(object, method, static_cast<Args>(pack.Get(i)->Unbox<std::remove_cv_t<std::remove_reference_t<Args>>>())...);
	}

	template <size_t... i>
	static bool CompatWithImpl(ArgumentPack const& pack, std::index_sequence<i...>)
	{
		return pack.Size() == sizeof...(i) && std::make_tuple((pack.GetType(i)->GetTypeIndex() == typeid(boxed_type_t<Args>))...) == std::make_tuple(std::bool_constant<i >= 0>::value...);
	}
};

template <typename Func>
class NonMemberMethod;

template <typename Ret, typename... Args>
class NonMemberMethod<Ret(*)(Args...)>
	: public natRefObjImpl<IMethod>
{
public:
	typedef Ret(*MethodType)(Args...);

	explicit NonMemberMethod(AccessSpecifier accessSpecifier, MethodType method)
		: m_AccessSpecifier{ accessSpecifier }, m_Func(method), m_Types(MethodHelper<MethodType>::GetType())
	{
	}

	AccessSpecifier GetAccessSpecifier() const noexcept override
	{
		return m_AccessSpecifier;
	}

	AccessSpecifier SetAccessSpecifier(AccessSpecifier accessSpecifier) noexcept override
	{
		return std::exchange(m_AccessSpecifier, accessSpecifier);
	}

	natRefPointer<Object> Invoke(ArgumentPack const& pack) override
	{
		return MethodHelper<MethodType>::Invoke(m_Func, pack);
	}

	bool CompatWith(ArgumentPack const& pack) const noexcept override
	{
		return MethodHelper<MethodType>::CompatWith(pack);
	}

	natRefPointer<IType> GetReturnType() const noexcept override
	{
		return m_Types.front();
	}

	size_t GetArgumentCount() const noexcept override
	{
		return m_Types.size() - 1;
	}

	natRefPointer<IType> GetArgumentType(size_t n) const noexcept override
	{
		return m_Types.at(n + 1);
	}

	MethodType Get() const noexcept
	{
		return m_Func;
	}

private:
	AccessSpecifier m_AccessSpecifier;
	MethodType m_Func;
	std::vector<natRefPointer<IType>> m_Types;
};

template <typename Func>
class MemberMethod;

template <typename Ret, typename Class, typename... Args>
class MemberMethod<Ret(Class::*)(Args...)>
	: public natRefObjImpl<IMemberMethod>
{
public:
	typedef	Ret(Class::*MethodType)(Args...);

	explicit MemberMethod(AccessSpecifier accessSpecifier, MethodType method)
		: m_AccessSpecifier{ accessSpecifier }, m_Func(method), m_Types(MethodHelper<MethodType>::GetType())
	{
	}

	AccessSpecifier GetAccessSpecifier() const noexcept override
	{
		return m_AccessSpecifier;
	}

	AccessSpecifier SetAccessSpecifier(AccessSpecifier accessSpecifier) noexcept override
	{
		return std::exchange(m_AccessSpecifier, accessSpecifier);
	}

	natRefPointer<Object> Invoke(natRefPointer<Object> object, ArgumentPack const& pack) override
	{
		if (object.Get() == nullptr)
		{
			nat_Throw(NullPointerException, _T("Object is nullptr."));
		}

		return MethodHelper<MethodType>::Invoke(object, m_Func, pack);
	}

	bool CompatWith(natRefPointer<Object> object, ArgumentPack const& pack) const noexcept override
	{
		if (object.Get() == nullptr)
		{
			return false;
		}

		return MethodHelper<MethodType>::CompatWith(object, pack);
	}

	natRefPointer<IType> GetReturnType() const noexcept override
	{
		return m_Types.front();
	}

	natRefPointer<IType> GetClassType() const noexcept override
	{
		return m_Types.at(1);
	}

	size_t GetArgumentCount() const noexcept override
	{
		return m_Types.size() - 2;
	}

	natRefPointer<IType> GetArgumentType(size_t n) const noexcept override
	{
		return m_Types.at(n + 2);
	}

	bool IsConstMemberMethod() const noexcept override
	{
		return false;
	}

	MethodType Get() const noexcept
	{
		return m_Func;
	}

private:
	AccessSpecifier m_AccessSpecifier;
	MethodType m_Func;
	std::vector<natRefPointer<IType>> m_Types;
};

template <typename Ret, typename Class, typename... Args>
class MemberMethod<Ret(Class::*)(Args...) const>
	: public natRefObjImpl<IMemberMethod>
{
public:
	typedef	Ret(Class::*MethodType)(Args...) const;

	explicit MemberMethod(AccessSpecifier accessSpecifier, MethodType method)
		: m_AccessSpecifier{ accessSpecifier }, m_Func(method), m_Types(MethodHelper<MethodType>::GetType())
	{
	}

	AccessSpecifier GetAccessSpecifier() const noexcept override
	{
		return m_AccessSpecifier;
	}

	AccessSpecifier SetAccessSpecifier(AccessSpecifier accessSpecifier) noexcept override
	{
		return std::exchange(m_AccessSpecifier, accessSpecifier);
	}

	natRefPointer<Object> Invoke(natRefPointer<Object> object, ArgumentPack const& pack) override
	{
		return MethodHelper<MethodType>::Invoke(object, m_Func, pack);
	}

	bool CompatWith(natRefPointer<Object> object, ArgumentPack const& pack) const noexcept override
	{
		return MethodHelper<MethodType>::CompatWith(object, pack);
	}

	natRefPointer<IType> GetReturnType() const noexcept override
	{
		return m_Types.front();
	}

	natRefPointer<IType> GetClassType() const noexcept override
	{
		return m_Types.at(1);
	}

	size_t GetArgumentCount() const noexcept override
	{
		return m_Types.size() - 2;
	}

	natRefPointer<IType> GetArgumentType(size_t n) const noexcept override
	{
		return m_Types.at(n + 2);
	}

	bool IsConstMemberMethod() const noexcept override
	{
		return true;
	}

	MethodType Get() const noexcept
	{
		return m_Func;
	}

private:
	AccessSpecifier m_AccessSpecifier;
	MethodType m_Func;
	std::vector<natRefPointer<IType>> m_Types;
};
